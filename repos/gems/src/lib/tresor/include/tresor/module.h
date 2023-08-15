/*
 * \brief  Framework for component internal modularization
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__MODULE_H_
#define _TRESOR__MODULE_H_

/* base includes */
#include <util/string.h>
#include <util/avl_tree.h>
#include <base/log.h>

/* tresor includes */
#include <tresor/verbosity.h>
#include <tresor/assertion.h>
#include <tresor/construct_in_buf.h>

namespace Tresor {

	using namespace Genode;

	using Module_id = uint64_t;
	using Module_request_id = uint64_t;

	enum {
		INVALID_MODULE_ID = ~0UL,
		INVALID_MODULE_REQUEST_ID = ~0UL,
	};

	enum Module_id_enum : Module_id
	{
		CRYPTO = 0,
		CLIENT_DATA = 1,
		TRUST_ANCHOR = 2,
		COMMAND_POOL = 3,
		BLOCK_IO = 4,
		CACHE = 5,
		META_TREE = 6,
		FREE_TREE = 7,
		VIRTUAL_BLOCK_DEVICE = 8,
		SUPERBLOCK_CONTROL = 9,
		BLOCK_ALLOCATOR = 10,
		VBD_INITIALIZER = 11,
		FT_INITIALIZER = 12,
		SB_INITIALIZER = 13,
		REQUEST_POOL = 14,
		SB_CHECK = 15,
		VBD_CHECK = 16,
		FT_CHECK = 17,
		FT_RESIZING = 18,
		MAX_MODULE_ID = 18,
	};

	char const *module_name(Module_id module_id);

	class Module_request;
	class Module_channel;
	class Module;
	class Module_composition;
}


class Tresor::Module_request : public Interface
{
	private:

		Module_id _src_module_id { INVALID_MODULE_ID };
		Module_request_id _src_request_id { INVALID_MODULE_REQUEST_ID };
		Module_id _dst_module_id { INVALID_MODULE_ID };
		Module_request_id _dst_request_id { INVALID_MODULE_REQUEST_ID };

	public:

		Module_request() { }

		Module_request(Module_id src_module_id,
		               Module_request_id src_request_id,
		               Module_id dst_module_id);

		void dst_request_id(Module_request_id id) { _dst_request_id = id; }

		String<32> src_request_id_str() const;

		String<32> dst_request_id_str() const;

		virtual void print(Output &) const = 0;

		virtual ~Module_request() { }


		/***************
		 ** Accessors **
		 ***************/

		Module_id src_module_id() const { return _src_module_id; }
		Module_request_id src_request_id() const { return _src_request_id; }
		Module_id dst_module_id() const { return _dst_module_id; }
		Module_request_id dst_request_id() const { return _dst_request_id; }
};


class Tresor::Module_channel : private Avl_node<Module_channel>
{
	friend class Module;
	friend class Avl_node<Module_channel>;
	friend class Avl_tree<Module_channel>;

	public:

		using Index = uint64_t;
		using State_uint = uint64_t;

	private:

		enum { GEN_REQ_BUF_SIZE = 4000 };

		enum Generated_request_state { NONE = 0, PENDING = 1, IN_PROGRESS = 2 };

		Module_request *_submitted_req_ptr { nullptr };
		Generated_request_state _gen_req_state { NONE };
		uint8_t _gen_req_buf[GEN_REQ_BUF_SIZE] { };
		State_uint _gen_req_complete_state { 0 };
		Index _idx { 0 };

		virtual void _generated_req_complete(State_uint state_uint) = 0;

		virtual void _request_submitted() = 0;

		virtual bool _request_complete() = 0;

		template <typename FUNC>
		void _with_channel(Index idx, FUNC && func)
		{
			if (idx != _idx) {
				Module_channel *chan_ptr { Avl_node<Module_channel>::child(idx > _idx) };
				if (chan_ptr)
					chan_ptr->_with_channel(idx, func);
			} else
				func(*this);
		}

		bool _try_submit_request(Module_request &req)
		{
			if (_submitted_req_ptr)
				return false;

			req.dst_request_id(_idx);
			_submitted_req_ptr = &req;
			_request_submitted();
			return true;
		}


		/**************
		 ** Avl_node **
		 **************/

		bool higher(Module_channel *ptr) { return ptr->_idx > _idx; }

	public:

		template <typename REQUEST, typename... ARGS>
		void generate_req(State_uint complete_state, bool &progress, ARGS &&... args)
		{
			ASSERT(_gen_req_state == NONE);
			static_assert(sizeof(REQUEST) <= GEN_REQ_BUF_SIZE);
			construct_at<REQUEST>(_gen_req_buf, args...);
			_gen_req_state = PENDING;
			_gen_req_complete_state = complete_state;
			progress = true;
		}

		Module_request *submitted_req_ptr() { return _submitted_req_ptr; }

		virtual ~Module_channel() { }
};


class Tresor::Module : public Interface
{
	friend class Module_channel;

	private:

		Avl_tree<Module_channel> _channels { };

		virtual bool _peek_completed_request(uint8_t *, size_t) { return false; }

		virtual void _drop_completed_request(Module_request &) { ASSERT_NEVER_REACHED; }

		virtual bool _peek_generated_request(uint8_t *, size_t) { return false; }

		virtual void _drop_generated_request(Module_request &) { ASSERT_NEVER_REACHED; }

		template <typename FUNC>
		void _with_channel(Module_channel::Index idx, FUNC && func)
		{
			if (_channels.first())
				_channels.first()->_with_channel(idx, func);
		}


		/****************************
		 ** Make class noncopyable **
		 ****************************/

		Module(Module const &) = delete;

		Module &operator = (Module const &) = delete;


	public:

		enum Handle_request_result { REQUEST_HANDLED, REQUEST_NOT_HANDLED };

		typedef Handle_request_result (*Handle_request_function)(Module_request &req);

		virtual bool ready_to_submit_request() { return false; };

		virtual void submit_request(Module_request &) { ASSERT_NEVER_REACHED; }

		virtual bool new_submit_request() { return false; }

		bool try_submit_request(Module_request &req)
		{
			bool success { false };
			_channels.for_each([&] (Module_channel const &const_chan) {
				Module_channel &chan { *const_cast<Module_channel *>(&const_chan) };
				if (success)
					return;

				if (chan._try_submit_request(req))
					success = true;
			});
			return success;
		}

		virtual void execute(bool &) { }

		template <typename FUNC>
		void for_each_generated_request(FUNC && handle_request)
		{
			uint8_t buf[4000];
			while (_peek_generated_request(buf, sizeof(buf))) {

				Module_request &req = *(Module_request *)buf;
				switch (handle_request(req)) {
				case Module::REQUEST_HANDLED:

					_drop_generated_request(req);
					break;

				case Module::REQUEST_NOT_HANDLED:

					return;
				}
			}
			_channels.for_each([&] (Module_channel const &const_chan) {

				Module_channel &chan { *const_cast<Module_channel *>(&const_chan) };
				if (chan._gen_req_state != Module_channel::PENDING)
					return;

				Module_request &req = *(Module_request *)chan._gen_req_buf;
				switch (handle_request(req)) {
				case Module::REQUEST_HANDLED:

					chan._gen_req_state = Module_channel::IN_PROGRESS;
					return;

				case Module::REQUEST_NOT_HANDLED: return;
				}
			});
		}

		virtual void generated_request_complete(Module_request &)
		{
			ASSERT_NEVER_REACHED;
		}

		bool new_generated_request_complete(Module_request &req)
		{
			bool result { false };
			Module_channel::Index const chan_idx { req.src_request_id() };
			_with_channel(chan_idx, [&] (Module_channel &chan) {

				if (chan._gen_req_state == Module_channel::NONE)
					return;

				ASSERT(chan._gen_req_state == Module_channel::IN_PROGRESS);
				chan._gen_req_state = Module_channel::NONE;
				chan._generated_req_complete(chan._gen_req_complete_state);
				result = true;
			});
			return result;
		}

		template <typename FUNC>
		void for_each_completed_request(FUNC && handle_request)
		{
			if (new_submit_request()) {
				_channels.for_each([&] (Module_channel const &const_chan) {
					Module_channel &chan { *const_cast<Module_channel *>(&const_chan) };
					if (chan._request_complete()) {
						ASSERT(chan._submitted_req_ptr);
						handle_request(*chan._submitted_req_ptr);
						chan._submitted_req_ptr = nullptr;
					}
				});
				return;
			}
			uint8_t buf[4000];
			while (_peek_completed_request(buf, sizeof(buf))) {
				Module_request &req = *(Module_request *)buf;
				handle_request(req);
				_drop_completed_request(req);
			}
		}

		template <typename T>
		void register_channels(T *channels_ptr, unsigned long num_channels)
		{
			for (unsigned long idx { 0 }; idx < num_channels; idx++) {
				Module_channel &chan { *static_cast<Module_channel *>(&channels_ptr[idx]) };
				chan._idx = idx;
				_channels.insert(&chan);
			}
		}

		Module() { }

		virtual ~Module() { }
};


class Tresor::Module_composition
{
	private:

		Module *_module_ptrs[MAX_MODULE_ID + 1] { };

	public:

		void add_module(Module_id module_id,
		                Module &mod)
		{
			ASSERT(module_id <= MAX_MODULE_ID);
			ASSERT(!_module_ptrs[module_id]);
			_module_ptrs[module_id] = &mod;
		}

		void remove_module(Module_id module_id)
		{
			ASSERT(module_id <= MAX_MODULE_ID);
			ASSERT(_module_ptrs[module_id]);
			_module_ptrs[module_id] = nullptr;
		}

		void execute_modules()
		{
			bool progress { true };
			while (progress) {

				progress = false;
				for (Module_id id { 0 }; id <= MAX_MODULE_ID; id++) {

					if (!_module_ptrs[id])
						continue;

					Module *module_ptr { _module_ptrs[id] };
					module_ptr->execute(progress);
					module_ptr->for_each_generated_request([&] (Module_request &req) {
						ASSERT(req.dst_module_id() <= MAX_MODULE_ID);
						ASSERT(_module_ptrs[req.dst_module_id()]);
						Module &dst_module { *_module_ptrs[req.dst_module_id()] };
						if (dst_module.new_submit_request()) {

							if (dst_module.try_submit_request(req)) {
								if (VERBOSE_MODULE_COMMUNICATION)
									log(
										module_name(id), " ", req.src_request_id_str(),
										" --", req, "--> ",
										module_name(req.dst_module_id()), " ",
										req.dst_request_id_str());

								progress = true;
								return Module::REQUEST_HANDLED;
							}
							if (VERBOSE_MODULE_COMMUNICATION)
								log(
									module_name(id), " ", req.src_request_id_str(),
									" --", req, "-| ",
									module_name(req.dst_module_id()));

							return Module::REQUEST_NOT_HANDLED;

						} else {

							if (!dst_module.ready_to_submit_request()) {

								if (VERBOSE_MODULE_COMMUNICATION)
									log(
										module_name(id), " ", req.src_request_id_str(),
										" --", req, "-| ",
										module_name(req.dst_module_id()));

								return Module::REQUEST_NOT_HANDLED;
							}
							dst_module.submit_request(req);

							if (VERBOSE_MODULE_COMMUNICATION)
								log(
									module_name(id), " ", req.src_request_id_str(),
									" --", req, "--> ",
									module_name(req.dst_module_id()), " ",
									req.dst_request_id_str());

							progress = true;
							return Module::REQUEST_HANDLED;
						}
					});
					module_ptr->for_each_completed_request([&] (Module_request &req) {
						ASSERT(req.src_module_id() <= MAX_MODULE_ID);
						if (VERBOSE_MODULE_COMMUNICATION)
							log(
								module_name(req.src_module_id()), " ",
								req.src_request_id_str(), " <--", req,
								"-- ", module_name(id), " ",
								req.dst_request_id_str());

						Module &src_module { *_module_ptrs[req.src_module_id()] };
						if (!src_module.new_generated_request_complete(req))
							src_module.generated_request_complete(req);

						progress = true;
					});
				}
			};
		}
};

#endif /* _TRESOR__MODULE_H_ */
