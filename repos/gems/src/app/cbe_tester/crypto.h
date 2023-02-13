/*
 * \brief  Implementation of the Crypto module API using the Crypto VFS API
 * \author Martin Stein
 * \date   2020-10-29
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CBE_TESTER__CRYPTO_H_
#define _CBE_TESTER__CRYPTO_H_

/* CBE includes */
#include <cbe/types.h>

/* CBE tester includes */
#include <vfs_utilities.h>
#include <module.h>

namespace Cbe {

	class Crypto_request : public Module_request
	{
		public:

			enum Type { INVALID, ADD_KEY, REMOVE_KEY };

		private:

			Type           _type    { INVALID };
			::Cbe::Request _request { };
			Key            _key     { };

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

			Crypto_request() { }

			Crypto_request(unsigned long  src_module_id,
			               unsigned long  src_request_id,
			               Type           type,
			               Request const &request,
			               Key     const &key)
			:
				Module_request { src_module_id, src_request_id, CRYPTO },
				_type          { type },
				_request       { request },
				_key           { key }
			{ }

			Type type() const { return _type; }
			Key const &key() const { return _key; }
	};
}

class Crypto : public Cbe::Module
{
	public:

		enum class Operation
		{
			INVALID,
			DECRYPT_BLOCK,
			ENCRYPT_BLOCK,
			ADD_KEY,
			REMOVE_KEY
		};

		enum class Result
		{
			SUCCEEDED,
			FAILED,
			RETRY_LATER
		};

	private:

		using Read_result = Vfs::File_io_service::Read_result;
		using Open_result = Vfs::Directory_service::Open_result;

		struct Key_directory
		{
			Vfs::Vfs_handle  *encrypt_handle { nullptr };
			Vfs::Vfs_handle  *decrypt_handle { nullptr };
			Genode::uint32_t  key_id         { 0 };
		};

		enum class Job_state
		{
			SUBMITTED,
			OP_WRITTEN_TO_VFS_HANDLE,
			READING_VFS_HANDLE_SUCCEEDED,
			COMPLETE
		};

		struct Job
		{
			Cbe::Request                      request        { };
			Vfs::Vfs_handle                  *handle         { nullptr };
			Job_state                         state          { Job_state::SUBMITTED };
			Operation                         op             { Operation::INVALID };
			Cbe::Crypto_cipher_buffer::Index  cipher_buf_idx { 0 };
			Cbe::Crypto_plain_buffer::Index   plain_buf_idx  { 0 };
			Cbe::Crypto_request               crypto_request { };
		};

		Vfs::Env                 &_env;
		Genode::String<32> const  _path;
		Vfs::Vfs_handle          &_add_key_handle;
		Vfs::Vfs_handle          &_remove_key_handle;
		Key_directory             _key_dirs[2] { { }, { } };
		Job                       _job         { };

		Key_directory &_get_unused_key_dir();

		Key_directory &_lookup_key_dir(Genode::uint32_t key_id);

		void _execute_decrypt_block(Job                       &job,
		                            Cbe::Crypto_plain_buffer  &plain_buf,
		                            Cbe::Crypto_cipher_buffer &cipher_buf,
		                            bool                      &progress);

		void _execute_encrypt_block(Job                       &job,
		                            Cbe::Crypto_plain_buffer  &plain_buf,
		                            Cbe::Crypto_cipher_buffer &cipher_buf,
		                            bool                      &progress);


		/************
		 ** Module **
		 ************/

		bool _peek_completed_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override
		{
			switch (_job.op) {
			case Operation::ADD_KEY:
			case Operation::REMOVE_KEY:

				if (_job.state == Job_state::COMPLETE) {

					if (sizeof(_job.crypto_request) > buf_size) {
						class Bad_size_2 { };
						throw Bad_size_2 { };
					}
					Genode::memcpy(buf_ptr, &_job.crypto_request, sizeof(_job.crypto_request));;
					return true;
				}

			case Operation::INVALID:
			case Operation::ENCRYPT_BLOCK:
			case Operation::DECRYPT_BLOCK:

				break;
			}
			return false;
		}

		void _drop_completed_request(Cbe::Module_request &/*mod_req*/) override
		{
			if (_job.op != Operation::ADD_KEY &&
			    _job.op != Operation::REMOVE_KEY) {

				class Bad_call_1 { };
				throw Bad_call_1 { };
			}
			if (_job.state != Job_state::COMPLETE) {
				class Bad_call_2 { };
				throw Bad_call_2 { };
			}
			_job.op = Operation::INVALID;
		}

		bool _peek_generated_request(Genode::uint8_t *,
		                             Genode::size_t   ) override
		{
			return false;
		}

		void _drop_generated_request(Cbe::Module_request &) override
		{
			class Bad_call { };
			throw Bad_call { };
		}

	public:

		Crypto(Vfs::Env &env, Genode::Xml_node const &crypto);

		bool request_acceptable() const;

		Result add_key(Cbe::Key const &key);

		Result remove_key(Cbe::Key::Id key_id);

		void submit_request(Cbe::Request               const &request,
		                    Operation                         op,
		                    Cbe::Crypto_plain_buffer::Index   plain_buf_idx,
		                    Cbe::Crypto_cipher_buffer::Index  cipher_buf_idx);

		Cbe::Request peek_completed_encryption_request() const;

		Cbe::Request peek_completed_decryption_request() const;

		void drop_completed_request();

		void execute(Cbe::Crypto_plain_buffer  &plain_buf,
		             Cbe::Crypto_cipher_buffer &cipher_buf,
		             bool                      &progress);


		/************
		 ** Module **
		 ************/

		void execute(bool &progress) override
		{
			switch (_job.op) {
			case Operation::ADD_KEY:

				if (_job.state == Job_state::SUBMITTED) {
					switch (add_key(_job.crypto_request.key())) {
					case Crypto::Result::SUCCEEDED:

						_job.crypto_request.success(true);
						_job.state = Job_state::COMPLETE;
						progress = true;
						break;

					case Crypto::Result::FAILED:

						class Add_key_failed_0 { };
						throw Add_key_failed_0 { };

					case Crypto::Result::RETRY_LATER:

						class Add_key_failed_1 { };
						throw Add_key_failed_1 { };
					}
				}
				break;

			case Operation::REMOVE_KEY:

				if (_job.state == Job_state::SUBMITTED) {
					switch (remove_key(_job.crypto_request.key().id)) {
					case Crypto::Result::SUCCEEDED:

						_job.crypto_request.success(true);
						_job.state = Job_state::COMPLETE;
						progress = true;
						break;

					case Crypto::Result::FAILED:

						class Remove_key_failed_0 { };
						throw Remove_key_failed_0 { };

					case Crypto::Result::RETRY_LATER:

						class Remove_key_failed_1 { };
						throw Remove_key_failed_1 { };
					}
				}
				break;

			case Operation::INVALID:
			case Operation::ENCRYPT_BLOCK:
			case Operation::DECRYPT_BLOCK:

				break;
			}
		}

		bool ready_to_submit_request() override
		{
			return _job.op == Operation::INVALID;
		}

		void submit_request(Cbe::Module_request &mod_request) override
		{
			Cbe::Crypto_request &crypto_req {
				*dynamic_cast<Cbe::Crypto_request *>(&mod_request) };

			switch (crypto_req.type()) {
			case Cbe::Crypto_request::ADD_KEY:

				crypto_req.dst_request_id(0);
				_job.state          = Job_state::SUBMITTED;
				_job.op             = Operation::ADD_KEY;
				_job.crypto_request = crypto_req;
				break;

			case Cbe::Crypto_request::REMOVE_KEY:

				crypto_req.dst_request_id(0);
				_job.state          = Job_state::SUBMITTED;
				_job.op             = Operation::REMOVE_KEY;
				_job.crypto_request = crypto_req;
				break;

			default:

				class Bad_type { };
				throw Bad_type { };
			}
		}

		void generated_request_complete(Cbe::Module_request &) override
		{
			class Bad_call { };
			throw Bad_call { };
		}
};

#endif /* _CBE_TESTER__CRYPTO_H_ */
