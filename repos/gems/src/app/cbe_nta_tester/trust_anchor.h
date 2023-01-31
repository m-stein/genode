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

/*
namespace Module {

	class Request
	{
		private:

			unsigned long _client_id;
			unsigned long _server_id;

		public:
	};
};

enum Trust_anchor_module_id { USER_REQUESTS, FILE_ACCESS };
*/




template <typename REQUEST, typename JOB, unsigned long NR_OF_JOBS>
class Module
{
	protected:

		JOB  _jobs[NR_OF_JOBS];
		JOB *_unused_job_ptr { _jobs };

	public:

		Module()
		{
			for (JOB &job : _jobs)
				job = JOB { };
		}

		bool ready_to_submit_request() { return _unused_job_ptr != nullptr; }

		void submit_request(REQUEST req)
		{
			if (_unused_job_ptr == nullptr) {
				class Invalid_call_to_submit_request { };
				throw Invalid_call_to_submit_request { };
			}
			_unused_job_ptr->use(req);
			_unused_job_ptr = nullptr;

			for (JOB &job : _jobs) {
				if (job.unused()) {
					_unused_job_ptr = &job;
					break;
				}
			}
		}

		REQUEST const *peek_completed_request() const
		{
			for (JOB const &job : _jobs) {
				if (job.completed())
					return &job.request;
			}
			return nullptr;
		}

		void drop_completed_request()
		{
			for (JOB &job : _jobs) {
				if (job.completed()) {
					job = JOB { };
					_unused_job_ptr = &job;
					return;
				}
			}
			class Invalid_call_to_drop_completed_request { };
			throw Invalid_call_to_drop_completed_request { };
		}

		REQUEST peek_generated_request() const
		{
			for (JOB const &job : _jobs) {
				REQUEST req;
				if (job.has_generated_request(req))
					return req;
			}
			return REQUEST { };
		}

		void drop_generated_request()
		{
			for (JOB const &job : _jobs) {
				if (job.drop_generated_request())
					return;
			}
			class Invalid_call_to_drop_generated_request { };
			throw Invalid_call_to_drop_generated_request { };
		}

		void generated_request_completed(REQUEST const &req)
		{
			for (JOB const &job : _jobs) {
				if (job.generated_request_completed(req))
					return;
			}
			class Invalid_call_to_generated_request_completed { };
			throw Invalid_call_to_generated_request_completed { };
		}
};




using File_path = Genode::String<128>;

struct File_access_request
{
	enum Type { INVALID, READ, WRITE };

	Type             type                  { INVALID };
	Vfs::Vfs_handle *file_ptr              { nullptr };
	Genode::off_t    file_offset           { 0 };
	char            *buf_ptr               { nullptr };
	Genode::size_t   buf_size              { 0 };
	Genode::size_t   nr_of_processed_bytes { 0 };
	bool             success               { false };
};

struct File_access_job
{
	enum State { INIT, IN_PROGRESS, COMPLETED };

	File_access_request request               { };
	State               state                 { INIT };
	Genode::size_t      nr_of_processed_bytes { 0 };

	bool unused() { return request.type == File_access_request::INVALID; }

	void use(File_access_request req) { request = req; }

	bool completed() const { return state == COMPLETED; }

	bool has_generated_request(File_access_request &) const { return false; }

	bool drop_generated_request() const { return false; }

	bool generated_request_completed(File_access_request const &) const { return false; }
};

class File_access : public Module<File_access_request, File_access_job, 1>
{
	public:

		using Job = File_access_job;
		using Request = File_access_request;

	private:

		using Read_result = Vfs::File_io_service::Read_result;
		using Write_result = Vfs::File_io_service::Write_result;

		Job      *_completed_job_ptr { _jobs };
		Vfs::Env &_vfs_env;

		void _call_file_write_once(Job  &job,
		                           bool &progress);

		void _execute_write(Job  &job,
		                    bool &progress);

		void _execute_read(Job  &job,
		                   bool &progress);

	public:

		File_access(Vfs::Env &vfs_env);

		void execute(bool &progress);
};




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

		File_access _file_access { _vfs_env };

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
