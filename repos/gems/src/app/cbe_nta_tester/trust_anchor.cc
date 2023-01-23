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

/* local includes */
#include <trust_anchor.h>

using namespace Genode;
using namespace Cbe;
using namespace Vfs;


File_access::File_access(Vfs::Env &vfs_env)
:
	_vfs_env { vfs_env }
{ }

void File_access::execute(bool &progress)
{
	for (Job &job : _jobs) {

		switch (job.request.type) {
		case Request::READ:    _execute_read(job, progress); break;
		case Request::INVALID:                               break;
		}
	}
}

void File_access::_execute_read(Job  &job,
                                bool &progress)
{
	Request &req { job.request };

	switch (job.state) {
	case Job::INIT:

		req.file_ptr->seek(req.file_offset);

		if (!req.file_ptr->fs().queue_read(req.file_ptr, req.buf_size))
			return;

		job.state = Job::READ_IN_PROGRESS;
		progress = true;
		return;

	case Job::READ_IN_PROGRESS:
	{
		file_size nr_of_read_bytes { 0 };
		Read_result const result {
			req.file_ptr->fs().complete_read(
				req.file_ptr, req.buf_ptr, req.buf_size, nr_of_read_bytes) };

		switch (result) {
		case Read_result::READ_QUEUED:
		case Read_result::READ_ERR_WOULD_BLOCK:

			return;

		case Read_result::READ_OK:

			error(nr_of_read_bytes);
			req.nr_of_processed_bytes = nr_of_read_bytes;
			req.success = true;
			job.state = Job::COMPLETED;
			progress = true;
			return;

		case Read_result::READ_ERR_INVALID:
		case Read_result::READ_ERR_IO:

			req.success = false;
			job.state = Job::COMPLETED;
			progress = true;
			return;
		}
	}
	case Job::COMPLETED:

		return;
	}
}



void Trust_anchor::_execute_write_read_operation(Vfs_handle        &file,
                                                 String<128> const &file_path,
                                                 char        const *write_buf,
                                                 char              *read_buf,
                                                 file_size          read_size,
                                                 bool              &progress)
{
	switch (_job.state) {
	case Job_state::WRITE_PENDING:

		file.seek(_job.file_offset);
		_job.state = Job_state::WRITE_IN_PROGRESS;
		progress = true;
		return;

	case Job_state::WRITE_IN_PROGRESS:
	{
		file_size nr_of_written_bytes { 0 };
		Write_result const result =
			file.fs().write(&file, write_buf + _job.file_offset,
			                _job.file_size, nr_of_written_bytes);
		switch (result) {

		case Write_result::WRITE_ERR_WOULD_BLOCK:
			return;

		case Write_result::WRITE_OK:

			_job.file_offset += nr_of_written_bytes;
			_job.file_size -= nr_of_written_bytes;

			if (_job.file_size > 0) {

				_job.state = Job_state::WRITE_PENDING;
				progress = true;
				return;
			}
			_job.state = Job_state::READ_PENDING;
			_job.file_offset = 0;
			_job.file_size = read_size;
			progress = true;
			return;

		default:

			_job.request.success(false);
			error("failed to write file ", file_path);
			_job.state = Job_state::COMPLETE;
			progress = true;
			return;
		}
	}
	case Job_state::READ_PENDING:

		file.seek(_job.file_offset);

		if (!file.fs().queue_read(&file, _job.file_size)) {
			return;
		}
		_job.state = Job_state::READ_IN_PROGRESS;
		progress = true;
		return;

	case Job_state::READ_IN_PROGRESS:
	{
		file_size nr_of_read_bytes { 0 };
		Read_result const result {
			file.fs().complete_read(
				&file, read_buf + _job.file_offset, _job.file_size,
				nr_of_read_bytes) };

		switch (result) {
		case Read_result::READ_QUEUED:
		case Read_result::READ_ERR_WOULD_BLOCK:

			return;

		case Read_result::READ_OK:

			_job.file_offset += nr_of_read_bytes;
			_job.file_size -= nr_of_read_bytes;
			_job.request.success(true);

			if (_job.file_size > 0) {

				_job.state = Job_state::READ_PENDING;
				progress = true;
				return;
			}
			_job.state = Job_state::COMPLETE;
			progress = true;
			return;

		default:

			_job.request.success(false);
			error("failed to read file ", file_path);
			_job.state = Job_state::COMPLETE;
			return;
		}
	}
	default:

		return;
	}
}


void Trust_anchor::_execute_write_operation(Vfs_handle        &file,
                                            String<128> const &file_path,
                                            char        const *write_buf,
                                            bool              &progress)
{
	switch (_job.state) {
	case Job_state::WRITE_PENDING:

		file.seek(_job.file_offset);
		_job.state = Job_state::WRITE_IN_PROGRESS;
		progress = true;
		return;

	case Job_state::WRITE_IN_PROGRESS:
	{
		file_size nr_of_written_bytes { 0 };
		Write_result const result =
			file.fs().write(
				&file, write_buf + _job.file_offset,
				_job.file_size, nr_of_written_bytes);

		switch (result) {

		case Write_result::WRITE_ERR_WOULD_BLOCK:
			return;

		case Write_result::WRITE_OK:

			_job.file_offset += nr_of_written_bytes;
			_job.file_size -= nr_of_written_bytes;

			if (_job.file_size > 0) {

				_job.state = Job_state::WRITE_PENDING;
				progress = true;
				return;
			}
			_job.state = Job_state::READ_PENDING;
			_job.file_offset = 0;
			_job.file_size = 0;
			progress = true;
			return;

		default:

			_job.request.success(false);
			error("failed to write file ", file_path);
			_job.state = Job_state::COMPLETE;
			progress = true;
			return;
		}
	}
	case Job_state::READ_PENDING:

		file.seek(_job.file_offset);

		if (!file.fs().queue_read(&file, _job.file_size)) {
			return;
		}
		_job.state = Job_state::READ_IN_PROGRESS;
		progress = true;
		return;

	case Job_state::READ_IN_PROGRESS:
	{
		file_size nr_of_read_bytes { 0 };
		Read_result const result {
			file.fs().complete_read(
				&file, _read_buf + _job.file_offset, _job.file_size,
				nr_of_read_bytes) };

		switch (result) {
		case Read_result::READ_QUEUED:
		case Read_result::READ_ERR_WOULD_BLOCK:

			return;

		case Read_result::READ_OK:

			_job.file_offset += nr_of_read_bytes;
			_job.file_size -= nr_of_read_bytes;
			_job.request.success(true);

			if (_job.file_size > 0) {

				_job.state = Job_state::READ_PENDING;
				progress = true;
				return;
			}
			_job.state = Job_state::COMPLETE;
			progress = true;
			return;

		default:

			_job.request.success(false);
			error("failed to read file ", file_path);
			_job.state = Job_state::COMPLETE;
			return;
		}
	}
	default:

		return;
	}
}


void Trust_anchor::_execute_read_operation(Vfs_handle        &file,
                                           String<128> const &file_path,
                                           char              *read_buf,
                                           bool              &progress)
{
	switch (_job.state) {
	case Job_state::READ_PENDING:

		file.seek(_job.file_offset);

		if (!file.fs().queue_read(&file, _job.file_size)) {
			return;
		}
		_job.state = Job_state::READ_IN_PROGRESS;
		progress = true;
		return;

	case Job_state::READ_IN_PROGRESS:
	{
		file_size nr_of_read_bytes { 0 };
		Read_result const result {
			file.fs().complete_read(
				&file, read_buf + _job.file_offset, _job.file_size,
				nr_of_read_bytes) };

		switch (result) {
		case Read_result::READ_QUEUED:
		case Read_result::READ_ERR_WOULD_BLOCK:

			return;

		case Read_result::READ_OK:

			_job.file_offset += nr_of_read_bytes;
			_job.file_size -= nr_of_read_bytes;
			_job.request.success(true);

			if (_job.file_size > 0) {

				_job.state = Job_state::READ_PENDING;
				progress = true;
				return;
			}
			_job.state = Job_state::COMPLETE;
			progress = true;
			error("Success ", Cstring { read_buf });
			return;

		default:

			_job.request.success(false);
			error("failed to write file ", file_path);
			_job.state = Job_state::COMPLETE;
			return;
		}
	}
	default:

		return;
	}
}


Trust_anchor::Trust_anchor(Vfs::Env       &vfs_env,
                           Xml_node const &xml_node)
:
	_vfs_env { vfs_env },
	_path    { xml_node.attribute_value("path", String<128>()) }
{
	Cbe::Trust_anchor_request request {
		Cbe::Trust_anchor_request::Operation::CREATE_KEY, false, 0
	};
	_job.request = request;
	_job.state = Job_state::READ_PENDING;
	_job.file_offset = 0;
	_job.file_size = 1;
}


bool Trust_anchor::request_acceptable() const
{
	return _job.request.operation() == Operation::INVALID;
}


void
Trust_anchor::submit_request_passphrase(Trust_anchor_request const &request,
                                        String<64>           const &passphrase)
{
	switch (request.operation()) {
	case Operation::INITIALIZE:

		_job.request      = request;
		_job.passphrase   = passphrase;
		_job.state        = Job_state::WRITE_PENDING;
		_job.file_offset  = 0;
		_job.file_size    = _job.passphrase.length();
		break;

	default:

		class Bad_operation { };
		throw Bad_operation { };
	}
}


void
Trust_anchor::
submit_request_key_plaintext_value(Trust_anchor_request const &request,
                                   Key_plaintext_value  const &key_plaintext_value)
{
	switch (request.operation()) {
	case Operation::ENCRYPT_KEY:

		_job.request             = request;
		_job.key_plaintext_value = key_plaintext_value;
		_job.state               = Job_state::WRITE_PENDING;
		_job.file_offset         = 0;
		_job.file_size           = sizeof(_job.key_plaintext_value.value) /
		                           sizeof(_job.key_plaintext_value.value[0]);
		break;

	default:

		class Bad_operation { };
		throw Bad_operation { };
	}
}


void
Trust_anchor::
submit_request_key_ciphertext_value(Trust_anchor_request const &request,
                                    Key_ciphertext_value const &key_ciphertext_value)
{
	switch (request.operation()) {
	case Operation::DECRYPT_KEY:

		_job.request              = request;
		_job.key_ciphertext_value = key_ciphertext_value;
		_job.state                = Job_state::WRITE_PENDING;
		_job.file_offset          = 0;
		_job.file_size            = sizeof(_job.key_ciphertext_value.value) /
		                            sizeof(_job.key_ciphertext_value.value[0]);
		break;

	default:

		class Bad_operation { };
		throw Bad_operation { };
	}
}


void Trust_anchor::submit_request_hash(Trust_anchor_request const &request,
                                       Hash                 const &hash)
{
	switch (request.operation()) {
	case Operation::SECURE_SUPERBLOCK:

		_job.request     = request;
		_job.hash        = hash;
		_job.state       = Job_state::WRITE_PENDING;
		_job.file_offset = 0;
		_job.file_size   = sizeof(_job.hash.values) /
		                 sizeof(_job.hash.values[0]);
		break;

	default:

		class Bad_operation { };
		throw Bad_operation { };
	}
}

void Trust_anchor::submit_request(Trust_anchor_request const &request)
{
	switch (request.operation()) {
	case Operation::LAST_SB_HASH:

		_job.request     = request;
		_job.state       = Job_state::READ_PENDING;
		_job.file_offset = 0;
		_job.file_size   = sizeof(_job.hash.values) /
		                   sizeof(_job.hash.values[0]);
		break;

	case Operation::CREATE_KEY:

		_job.request     = request;
		_job.state       = Job_state::READ_PENDING;
		_job.file_offset = 0;
		_job.file_size   = sizeof(_job.key_plaintext_value.value) /
		                   sizeof(_job.key_plaintext_value.value[0]);
		break;

	default:

		class Bad_operation { };
		throw Bad_operation { };
	}
}


void Trust_anchor::execute(bool &progress)
{
	static bool done = false;
	if (!done) {
		if (_file_access.ready_to_submit_request()) {

			using Request = File_access::Request;
			_file_access.submit_request(
				Request {
					Request::READ, &_responses_file, 0, _responses_read_buf,
					sizeof(_responses_read_buf_storage) - 1 });

			done = true;
		}
	}
	_file_access.execute(progress);
	{
		using Request = File_access::Request;
		Request const *req { _file_access.peek_completed_request() };
		if (req) {
			if (req->success) {

				_responses_read_buf_storage[req->nr_of_processed_bytes] = 0;
				error("success reading ", req->nr_of_processed_bytes,
				      " bytes from file: ", Cstring { _responses_read_buf });
				_file_access.drop_completed_request();
			} else {
				error("failed reading responses file ");
			}
		}
	}
/*
	switch (_job.request.operation()) {
	case Operation::INITIALIZE:

		_execute_write_operation(
			_initialize_file, _initialize_path,
			_job.passphrase.string(), progress);

		break;

	case Operation::SECURE_SUPERBLOCK:

		_execute_write_operation(
			_hashsum_file, _hashsum_path,
			_job.hash.values, progress);

		break;

	case Operation::LAST_SB_HASH:

		_execute_read_operation(
			_hashsum_file, _hashsum_path,
			_job.hash.values, progress);

		break;

	case Operation::CREATE_KEY:

		_execute_read_operation(
			_responses_file, _responses_path,
			_responses_read_buf, progress);

		break;

	case Operation::ENCRYPT_KEY:

		_execute_write_read_operation(
			_encrypt_file, _encrypt_path,
			_job.key_plaintext_value.value,
			_job.key_ciphertext_value.value,
			sizeof(_job.key_ciphertext_value.value) /
				sizeof(_job.key_ciphertext_value.value[0]),
			progress);

		break;

	case Operation::DECRYPT_KEY:

		_execute_write_read_operation(
			_decrypt_file, _decrypt_path,
			_job.key_ciphertext_value.value,
			_job.key_plaintext_value.value,
			sizeof(_job.key_plaintext_value.value) /
				sizeof(_job.key_plaintext_value.value[0]),
			progress);

		break;

	case Operation::INVALID:

		break;

	default:

		class Bad_operation { };
		throw Bad_operation { };
	}
*/
}


Trust_anchor_request Trust_anchor::peek_completed_request() const
{
	if (_job.state != Job_state::COMPLETE) {

		return Trust_anchor_request { };
	}
	return _job.request;
}


Hash const &Trust_anchor::peek_completed_hash() const
{
	if (_job.request.operation() != Operation::LAST_SB_HASH) {

		class Bad_operation { };
		throw Bad_operation { };
	}
	if (_job.state != Job_state::COMPLETE) {

		class Bad_state { };
		throw Bad_state { };
	}
	return _job.hash;
}


Key_plaintext_value const &
Trust_anchor::peek_completed_key_plaintext_value() const
{
	if (_job.request.operation() != Operation::CREATE_KEY &&
	    _job.request.operation() != Operation::DECRYPT_KEY) {

		class Bad_operation { };
		throw Bad_operation { };
	}
	if (_job.state != Job_state::COMPLETE) {

		class Bad_state { };
		throw Bad_state { };
	}
	return _job.key_plaintext_value;
}


Key_ciphertext_value const &
Trust_anchor::peek_completed_key_ciphertext_value() const
{
	if (_job.request.operation() != Operation::ENCRYPT_KEY) {

		class Bad_operation { };
		throw Bad_operation { };
	}
	if (_job.state != Job_state::COMPLETE) {

		class Bad_state { };
		throw Bad_state { };
	}
	return _job.key_ciphertext_value;
}


void Trust_anchor::drop_completed_request()
{
	if (_job.state != Job_state::COMPLETE) {

		class Bad_state { };
		throw Bad_state { };
	}
	_job.request = Trust_anchor_request { };
}
