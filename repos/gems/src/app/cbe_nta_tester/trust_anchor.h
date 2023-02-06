/*
 * \brief  Implementation of the TA module API using the TA VFS API
 * \author Martin Stein
 * \date   2020-10-29
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CBE_TESTER__TRUST_ANCHOR_H_
#define _CBE_TESTER__TRUST_ANCHOR_H_

/* os includes */
#include <os/vfs.h>

/* CBE includes */
#include <cbe/types.h>

/* CBE tester includes */
#include <vfs_utilities.h>

/* local includes */
#include <file_access.h>


using File_path = Genode::String<128>;

namespace Cbe_tester
{
	class Frust_anchor;
	class Frust_anchor_request;
	class Frust_anchor_channel;
}

class Cbe_tester::Frust_anchor_request
{
	public:

		enum Type {
			INVALID    = 0,
			CREATE_KEY = 1
		};

	private:

		friend class Frust_anchor;
		friend class Frust_anchor_channel;

		Type _type { Type::INVALID };

		Frust_anchor_request() { }

	public:

		Frust_anchor_request(Type type)
		:
			_type { type }
		{ }
};

class Cbe_tester::Frust_anchor_channel
{
	private:

		friend class Frust_anchor;

		enum State { INACTIVE, IN_PROGRESS, COMPLETED };

		State                _state   { INACTIVE };
		Frust_anchor_request _request { };
		unsigned long        _version { 0 };

	public:

		Frust_anchor_request const &request() const { return _request; }
};

class Cbe_tester::Frust_anchor
{
	private:

		enum State { INACTIVE, IN_PROGRESS };

		using Request = Frust_anchor_request;
		using Channel = Frust_anchor_channel;

		Channel             _channel[4];

		Vfs::Env            &_vfs_env;
		Genode::String<128>  _path;
		Genode::String<128>  _requests_path         { _path, "/requests" };
		Vfs::Vfs_handle     &_requests_file         { vfs_open_rw(_vfs_env, { _requests_path }) };
		unsigned long        _requests_file_version { 0 };
		char                 _requests_buf[512];
		unsigned long        _requests_buf_version  { _requests_file_version };
		File_access_request  _requests_write_req    { File_access_request::WRITE, _requests_file, 0, _requests_buf, sizeof(_requests_buf) };
		unsigned long        _requests_version      { _requests_buf_version + 1 };

		void _write_requests_buf()
		{
			Genode::Xml_generator xml {
				_requests_buf, sizeof(_requests_buf), "requests",
				[&] () {
					for (Channel const &channel : _channels) {
						xml.node("request", [&] () {
							xml.attribute(
								"type",
								(unsigned long)channel.request().type());
						});
					}
				}
			};
		}

	public:

		bool ready_to_submit_request()
		{
			if (_state != INACTIVE)
				return false;

			for (Channel &channel : _channels) {
				if (channel._state == INACTIVE)
					return true;
			}
			return false;
		}

		void submit_request(Request &request)
		{
			if (_state != INACTIVE)
				throw -1;

			for (Channel &channel : _channels) {
				if (channel._state == INACTIVE) {
					channel._request = request;
					channel._version = _requests_buf_version + 1;
					channel._state = IN_PROGRESS;
					return;
				}
			}
			throw -1;
		}

		template <typename FUNC>
		void with_completed_request(FUNC && functor) const
		{
			for (Channel &channel : _channels) {
				if (channel._state == COMPLETED) {
					functor(channel._request);
					return;
				}
			}
		}

		void execute(bool &progress)
		{
			requests_buf_outdated { false };
			for (Channel &channel : _channels) {
				if (channel._version > _requests_buf_version) {
					requests_buf_outdated = true;
				}
			}
			if (requests_buf_outdated) {
				_write_requests_buf();
				_requests_buf_version = _requests_version;
				_requests_version++;
			}
			if (_requests_buf_version > _requests_file_version &&
			    _requests_file_state == INACTIVE) {

				_requests_file_state = WRITE_PENDING;
				progress = true;
			}
		}

		template <typename FUNC>
		void with_generated_request(FUNC && functor) const
		{
			if (_requests_file_state == WRITE_PENDING) {
				functor(_requests_write_req);
				_requests_file_state == WRITE_IN_PROGRESS;
				return;
			}
		}

		void generated_request_completed(unsigned long  dst_id,
		                                 void          *req_ptr)
		{
			if (dst_id == _file_access_id) {

				File_access_request &req {
					*static_cast<File_access_request*>(req_ptr) };

				if (req._type == File_access_request::WRITE) {

					if (_requests_file_state != WRITE_IN_PROGRESS)
						throw -1;

					_requests_file_state = INACTIVE;
					_requests_file_version = _requests_buf_version;
					return;
				}
			}
			throw -1;
		}

		Frust_anchor(Vfs::Env            &vfs_env,
		             Genode::String<128> &path)
		:
			_vfs_env { vfs_env },
			_path    { path }
		{ }
};

/*

class Cbe_tester::Frust_anchor_request
{
	public:

		enum Type {
			INVALID           = 0,
			CREATE_KEY        = 1,
			SECURE_SUPERBLOCK = 2,
			ENCRYPT_KEY       = 3,
			DECRYPT_KEY       = 4,
			LAST_SB_HASH      = 5,
			INITIALIZE        = 6,
		};

	private:

		friend class Frust_anchor;
		friend class Frust_anchor_channel;

		Type             _type    { Type::INVALID };
		bool             _success { false };
		Genode::uint32_t _tag     { 0 };

		Frust_anchor_request() { }

	public:

		Frust_anchor_request(Type             type,
		                     Genode::uint32_t tag)
		:
			_type { type },
			_tag  { tag }
		{ }

		Type type() const { return _type; }
		bool success() const { return _success; }
		Genode::uint32_t tag() const { return _tag; }
};





class Cbe_tester::Frust_anchor_channel
{
	private:

		friend class Frust_anchor;

		enum State
		{
			SUBMITTED_WRITE_REQUESTS_NEEDED,
			SUBMITTED_WRITE_REQUESTS_PENDING,
			SUBMITTED_WRITE_REQUESTS_IN_PROGRESS,
			IN_PROGRESS,
			COMPLETED,
			INACTIVE_WRITE_REQUESTS_NEEDED,
			INACTIVE_WRITE_REQUESTS_PENDING,
			INACTIVE_WRITE_REQUESTS_IN_PROGRESS,
			INACTIVE
		};

		State                _state   { INACTIVE };
		Frust_anchor_request _request { };

	public:

		bool unused() { return _state == INACTIVE; }

		void use(Frust_anchor_request req)
		{
			_request = req;
			_state = REQ_SUBMITTED_WRITE_REQUESTS;
		}

		bool completed() const { return _state == COMPLETED; }

		Frust_anchor_request const &request() const { return _request; }
};


class Cbe_tester::Frust_anchor
:
	public Genode::Request_processor<Frust_anchor,
	                                 Frust_anchor_request,
	                                 Frust_anchor_channel, 4>
{
	private:

		using Request = Frust_anchor_request;
		using Channel = Frust_anchor_channel;

		Vfs::Env                  &_vfs_env;
		Genode::String<128> const  _path;
		Genode::String<128> const  _requests_path           { _path, "/requests" };
		Vfs::Vfs_handle           &_requests_file           { vfs_open_rw(_vfs_env, { _requests_path }) };
		char                       _requests_write_buf_storage[512];
		char *                     _requests_write_buf      { _requests_write_buf_storage };
		bool                       _requests_write_required { false };
		Channel                   *_completed_channel_ptr   { _channels };
		

		void _update_requests_write_buf()
		{
			Genode::Xml_generator xml {
				_requests_write_buf_storage,
				sizeof(_requests_write_buf_storage),
				"requests",
				[&] () {
					xml.node("requests", [&] () {
						for (Channel const &channel : _channels) {

							if (channel.unused())
								continue;

							xml.node("request", [&] () {
								xml.attribute("type", (unsigned long)channel.request().type());
							});
						}
					});
				}
			};

	public:

		Frust_anchor(Vfs::Env               &vfs_env,
		             Genode::Xml_node const &xml_node)
		:
			_vfs_env { vfs_env },
			_path    { xml_node.attribute_value("path", Genode::String<128>()) }
		{ }

		void execute_one_step(bool &progress)
		{
			bool update_requests_write_buf { false };
			for (Channel &channel : _channels) {
				switch (channel._state) {
				case SUBMITTEDWRITE_REQUESTS_NEEDED:

					update_requests_write_buf = true;
					channel.state = SUBMITTED_WRITE_REQUESTS_PENDING;
					progress = true;
					break;

				case INACTIVEWRITE_REQUESTS_NEEDED: break;
					update_requests_write_buf = true;
					channel.state = SUBMITTED_WRITE_REQUESTS_PENDING;
					progress = true;
					break;

				case SUBMITTED_WRITE_REQUESTS_PENDING:
				case SUBMITTED_WRITE_REQUESTS_IN_PROGRESS:
				case IN_PROGRESS:
				case COMPLETED:
				case INACTIVE_WRITE_REQUESTS_PENDING:
				case INACTIVE_WRITE_REQUESTS_IN_PROGRESS:
				case INACTIVE:
				}
			}
			if (update_requests_write_buf)
				_update_requests_write_buf(
					_requests_write_buf_storage,
					sizeof(_requests_write_buf_storage));
		}

		bool has_generated_request(unsigned long &dst_id) const
		{
			for (Channel const &channel : _channels) {
				switch (channel[id]._state) {
				case SUBMITTED_WRITE_REQUESTS_PENDING:

					dst_id = _file_access_id;
					return true;

				case INACTIVE_WRITE_REQUESTS_PENDING:

					dst_id = _file_access_id;
					return true;

				case SUBMITTED_WRITE_REQUESTS_NEEDED:
				case SUBMITTED_WRITE_REQUESTS_IN_PROGRESS:
				case IN_PROGRESS:
				case COMPLETED:
				case INACTIVE_WRITE_REQUESTS_NEEDED:
				case INACTIVE_WRITE_REQUESTS_IN_PROGRESS:
				case INACTIVE:
				}
			}
			return false;
		}

		void submit_generated_request(unsigned long  dst_id,
		                              char          *dst_ptr)
		{
			bool submit_write_requests;
			for (Channel const &channel : _channels) {
				switch (channel[id]._state) {
				case SUBMITTED_WRITE_REQUESTS_PENDING:

					if (dst_id == _file_access_id) {
						submit_write_requests = true;
						channel.state = SUBMITTED_WRITE_REQUESTS_IN_PROGRESS;
					}
					break;

				case INACTIVE_WRITE_REQUESTS_PENDING:

					if (dst_id == _file_access_id) {
						submit_write_requests = true;
						channel.state = INACTIVE_WRITE_REQUESTS_IN_PROGRESS;
					}
					break;

				case SUBMITTED_WRITE_REQUESTS_NEEDED:
				case SUBMITTED_WRITE_REQUESTS_IN_PROGRESS:
				case IN_PROGRESS:
				case COMPLETED:
				case INACTIVE_WRITE_REQUESTS_NEEDED:
				case INACTIVE_WRITE_REQUESTS_IN_PROGRESS:
				case INACTIVE:
				}
			}
			if (submit_write_requests) {

				*static_cast<File_access_request*>(dst_ptr) =
					File_access_request {
						File_access_request::WRITE, _requests_file, 0,
						_requests_write_buf,
						sizeof(_requests_write_buf_storage) };

				return;
			}
		}
};
*/

class Trust_anchor
{
	private:

		using Read_result = Vfs::File_io_service::Read_result;
		using Write_result = Vfs::File_io_service::Write_result;
		using Operation = Cbe::Trust_anchor_request::Operation;

		enum Job_state
		{
			WRITE_PENDING,
			WRITE_IN_PROGRESS,
			READ_PENDING,
			READ_IN_PROGRESS,
			COMPLETE
		};

		struct Job
		{
			Cbe::Trust_anchor_request request              { };
			Job_state                 state                { Job_state::COMPLETE };
			Genode::String<64>        passphrase           { };
			Cbe::Hash                 hash                 { };
			Cbe::Key_plaintext_value  key_plaintext_value  { };
			Cbe::Key_ciphertext_value key_ciphertext_value { };
			Vfs::file_offset          file_offset          { 0 };
			Vfs::file_size            file_size            { 0 };
		};

		Vfs::Env                  &_vfs_env;
		char                       _read_buf[64];
		Genode::String<128> const  _path;
/*
		Genode::String<128> const  _decrypt_path      { _path, "/decrypt" };
		Vfs::Vfs_handle           &_decrypt_file      { vfs_open_rw(_vfs_env, { _decrypt_path }) };
		Genode::String<128> const  _encrypt_path      { _path, "/encrypt" };
		Vfs::Vfs_handle           &_encrypt_file      { vfs_open_rw(_vfs_env, { _encrypt_path }) };
		Genode::String<128> const  _generate_key_path { _path, "/generate_key" };
		Vfs::Vfs_handle           &_generate_key_file { vfs_open_rw(_vfs_env, { _generate_key_path }) };
		Genode::String<128> const  _initialize_path   { _path, "/initialize" };
		Vfs::Vfs_handle           &_initialize_file   { vfs_open_rw(_vfs_env, { _initialize_path }) };
		Genode::String<128> const  _hashsum_path      { _path, "/hashsum" };
		Vfs::Vfs_handle           &_hashsum_file      { vfs_open_rw(_vfs_env, { _hashsum_path }) };
*/
		Job                        _job               { };

		Cbe_tester::File_access _file_access { _vfs_env };

		Genode::String<128> const            _responses_path          { _path, "/responses" };
		Vfs::Vfs_handle                     &_responses_file          { vfs_open_rw(_vfs_env, { _responses_path }) };
		char                                 _responses_read_buf_storage[512];
		char *                               _responses_read_buf      { _responses_read_buf_storage };
		bool                                 _responses_read_required { true };
		Genode::Watch_handler<Trust_anchor>  _responses_handler       { _vfs_env.root_dir(), _responses_path, _vfs_env.alloc(), *this, &Trust_anchor::_handle_responses };

		Genode::String<128> const  _requests_path { _path, "/requests" };
		Vfs::Vfs_handle           &_requests_file { vfs_open_rw(_vfs_env, { _requests_path }) };
		char                       _requests_write_buf_storage[512];
		char *                     _requests_write_buf { _requests_write_buf_storage };

		void _handle_responses() { _responses_read_required = true; }

		void _execute_write_read_operation(Vfs::Vfs_handle           &file,
		                                   Genode::String<128> const &file_path,
		                                   char                const *write_buf,
		                                   char                      *read_buf,
		                                   Vfs::file_size             read_size,
		                                   bool                      &progress);

		void _execute_write_operation(Vfs::Vfs_handle           &file,
		                              Genode::String<128> const &file_path,
		                              char                const *write_buf,
		                              bool                      &progress);

		void _execute_read_operation(Vfs::Vfs_handle           &file,
		                             Genode::String<128> const &file_path,
		                             char                      *read_buf,
		                             bool                      &progress);

		Trust_anchor(const Trust_anchor&) = delete;
		Trust_anchor &operator=(const Trust_anchor&) = delete;

	public:

		Trust_anchor(Vfs::Env                          &vfs_env,
		             Genode::Xml_node            const &xml_node);

		bool request_acceptable() const;

		void submit_request_passphrase(Cbe::Trust_anchor_request const &request,
		                               Genode::String<64>        const &passphrase);

		void
		submit_request_key_plaintext_value(Cbe::Trust_anchor_request const &request,
		                                   Cbe::Key_plaintext_value  const &key_plaintext_value);

		void
		submit_request_key_ciphertext_value(Cbe::Trust_anchor_request const &request,
		                                    Cbe::Key_ciphertext_value const &key_ciphertext_value);

		void submit_request_hash(Cbe::Trust_anchor_request const &request,
		                         Cbe::Hash                 const &hash);

		void submit_request(Cbe::Trust_anchor_request const &request);

		void execute(bool &progress);

		Cbe::Trust_anchor_request peek_completed_request() const;

		Cbe::Hash const &peek_completed_hash() const;

		Cbe::Key_plaintext_value const &
		peek_completed_key_plaintext_value() const;

		Cbe::Key_ciphertext_value const &
		peek_completed_key_ciphertext_value() const;

		void drop_completed_request();
};

#endif /* _CBE_TESTER__TRUST_ANCHOR_H_ */
