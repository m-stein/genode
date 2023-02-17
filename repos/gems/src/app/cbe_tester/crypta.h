
#ifndef _CRYPTA_H_
#define _CRYPTA_H_

/* gems includes */
#include <cbe/types.h>

/* cbe tester includes */
#include <module.h>

namespace Cbe
{
	class Crypta;
	class Crypta_request;
	class Crypta_channel;

	enum { KEY_SIZE = 32 };
	enum { PRIM_BUF_SIZE = 128 };
}

class Cbe::Crypta_request : public Module_request
{
	public:

		enum Type
		{
			INVALID = 0,
			ADD_KEY = 1,
			REMOVE_KEY = 2,
			DECRYPT = 3,
			ENCRYPT = 4,
			DECRYPT_CLIENT_DATA = 5,
			ENCRYPT_CLIENT_DATA = 6
		};

	private:

		friend class Crypta;
		friend class Crypta_channel;

		Type             _type                    { INVALID };
		Genode::uint32_t _key_id                  { 0 };
		unsigned long    _data_idx                { 0 };
		Genode::uint64_t _block_addr              { 0 };
		::Cbe::Request   _request                 { };
		Genode::uint8_t  _prim[PRIM_BUF_SIZE]     { };
		Genode::uint8_t  _key_plaintext[KEY_SIZE] { };
		bool             _success                 { false };

	public:

		char const *type_name() override
		{
			switch (_type) {
			case INVALID: return "invalid";
			case ADD_KEY: return "add_key";
			case REMOVE_KEY: return "remove_key";
			default: break;
			}
			return "?";
		}

		Crypta_request() { }

		Type type() const { return _type; }


		/*****************************************************
		 ** can be removed once the cbe translation is done **
		 *****************************************************/

		Crypta_request(unsigned long src_module_id,
		               unsigned long src_request_id)
		:
			Module_request { src_module_id, src_request_id, CRYPTA }
		{ }

		static void create(
			void     * /*buf_ptr*/,
			Genode::size_t     /*buf_size*/,
			Genode::size_t     req_type,
			Genode::uint64_t   /*req_blk_nr*/,
			void     * /*prim_ptr*/,
			size_t     /*prim_size*/,
			Genode::uint32_t   /*key_id*/,
			void     * /*key_plain_ptr*/,
			Genode::uint64_t   /*pba*/,
			Genode::uint64_t   /*vba*/,
			void     * /*plain_blk_ptr*/,
			void     * /*cipher_blk_ptr*/);

		void *prim() override { return (void *)&_prim; }
		void *result_blk_ptr()
		{
			switch (_type) {
			case DECRYPT:
				class Not_yet_implemented_1 { };
				throw Not_yet_implemented_1 { };
			case ENCRYPT:
				class Not_yet_implemented_2 { };
				throw Not_yet_implemented_2 { };
			case INVALID:
			case ADD_KEY:
			case REMOVE_KEY:
			case DECRYPT_CLIENT_DATA:
			case ENCRYPT_CLIENT_DATA:
				break;
			}
			return nullptr;
		}
};

class Cbe::Crypta_channel
{
	private:

		friend class Crypta;

		enum State { INACTIVE, PENDING, IN_PROGRESS, COMPLETE };

		State          _state   { INACTIVE };
		Crypta_request _request { };

	public:

		Crypta_request const &request() const { return _request; }
};

class Cbe::Crypta : public Module
{
	private:

		using Request = Crypta_request;
		using Channel = Crypta_channel;

		enum { NR_OF_CHANNELS = 4 };

		Channel _channels[NR_OF_CHANNELS];


		/************
		 ** Module **
		 ************/

		bool _peek_completed_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override
		{
			for (Channel &channel : _channels) {
				if (channel._state == Channel::COMPLETE) {
					if (sizeof(channel._request) > buf_size) {
						class Bad_size_2 { };
						throw Bad_size_2 { };
					}
					Genode::memcpy(buf_ptr, &channel._request, sizeof(channel._request));;
					return true;
				}
			}
			return false;
		}

		void _drop_completed_request(Module_request &req) override
		{
			unsigned long id { 0 };
			id = req.dst_request_id();
			if (id >= NR_OF_CHANNELS) {

				class Bad_id { };
				throw Bad_id { };
			}
			if (_channels[id]._state != Channel::COMPLETE) {

				class Bad_state { };
				throw Bad_state { };
			}
			_channels[id]._state = Channel::INACTIVE;
		}

		bool _peek_generated_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override;

		void _drop_generated_request(Module_request &mod_req) override;

	public:

		bool ready_to_submit_request() override
		{
			for (Channel &channel : _channels) {
				if (channel._state == Channel::INACTIVE)
					return true;
			}
			return false;
		}

		void submit_request(Module_request &req) override
		{
			for (unsigned long id { 0 }; id < NR_OF_CHANNELS; id++) {
				if (_channels[id]._state == Channel::INACTIVE) {
					req.dst_request_id(id);
					_channels[id]._request = *dynamic_cast<Request *>(&req);
					_channels[id]._state = Channel::PENDING;
					return;
				}
			}
			class Invalid_call { };
			throw Invalid_call { };
		}


		/************
		 ** Module **
		 ************/

		void execute(bool &) override
		{
			for (Channel &channel : _channels) {
				if (channel._state != Channel::INACTIVE) {
					switch (channel._request._type) {
					case Request::ADD_KEY:
					case Request::REMOVE_KEY:
						break;
					default:
						class Bad_request_type { };
						throw Bad_request_type { };
					}
				}
			}
		}

		void generated_request_complete(Module_request &mod_req) override
		{
			unsigned long const id { mod_req.src_request_id() };
			if (id >= NR_OF_CHANNELS) {
				class Bad_id { };
				throw Bad_id { };
			}
			if (_channels[id]._state != Channel::IN_PROGRESS) {
				class Bad_state { };
				throw Bad_state { };
			}
			_channels[id]._request.success(mod_req.success());
			_channels[id]._state = Channel::COMPLETE;
		}

		Crypta()
		{
			for (Channel &channel : _channels)
				channel = Channel { };
		}
};

#endif /* _CRYPTA_H_ */
