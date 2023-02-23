/*
 * \brief  Integration of the Consistent Block Encrypter (CBE)
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2020-11-10
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CBE_LIBRARY_H_
#define _CBE_LIBRARY_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/output.h>

/* CBE includes */
#include <cbe/types.h>
#include <cbe/spark_object.h>


extern "C" void cbe_cxx_init();
extern "C" void cbe_cxx_final();


namespace Cbe {

	using namespace Genode;

	class Library;

	Genode::uint32_t object_size(Library const &);

} /* namespace Cbe */


class Cbe::Library : public Cbe::Spark_object<353944>
{
	private:

		/*
		 * Ada/SPARK compatible bindings
		 *
		 * Ada functions cannot have out parameters. Hence we call Ada
		 * procedures that return the 'progress' result as last out parameter.
		 */

		void _has_io_request(Request &, Io_buffer::Index &) const;

		void _info(Info &) const;

	public:

	Library();

	/**
	 * Get highest virtual-block-address useable by the current active snapshot
	 *
	 * \return  highest addressable virtual-block-address
	 */
	Virtual_block_address max_vba() const;

	/**
	 * Get information about the CBE
	 *
	 * \return  information structure
	 */
	Info info() const
	{
		Info inf { };
		_info(inf);
		return inf;
	}

	void execute(Io_buffer &io_buf);

	/**
	 * Return whether the last call to 'execute' has made progress or not
	 */
	bool execute_progress() const;

	/**
	 * Check if the CBE can accept a new requeust
	 *
	 * \return true if a request can be accepted, otherwise false
	 */
	bool client_request_acceptable() const;

	/**
	 * Submit a new request
	 *
	 * This method must only be called after executing 'request_acceptable'
	 * returned true.
	 *
	 * \param request  block request
	 */
	void submit_client_request(Request const &request, uint32_t id);

	/**
	 * Check for any completed request
	 *
	 * \return a valid block request will be returned if there is an
	 *         completed request, otherwise an invalid one
	 */
	Request peek_completed_client_request() const;

	/**
	 * Drops the completed request
	 *
	 * This method must only be called after executing
	 * 'peek_completed_request' returned a valid request.
	 *
	 */
	void drop_completed_client_request(Request const &req);

	/*
	 * Backend block I/O
	 */

	/**
	 * Submit read request data from the backend block session to the CBE
	 *
	 * The given data will be transfered to the CBE.
	 *
	 * \param  request  reference to the request from the CBE
	 * \param  data     reference to the data associated with the
	 *                  request
	 *
	 * \return  true if the CBE acknowledged the request
	 */
	void io_request_completed(Io_buffer::Index const &data_index,
	                          bool             const  success);

	/**
	 * Return a write request for the backend block session
	 *
	 * \param result  valid request in case the is one pending that
	 *                needs data, otherwise an invalid one is returned
	 */
	Request has_io_request(Io_buffer::Index &data_index) const
	{
		Request result { };
		_has_io_request(result, data_index);
		return result;
	}
	void has_io_request(Request &req, Io_buffer::Index &data_index) const
	{
		_has_io_request(req, data_index);
	}

	/**
	 * Obtain data for write request for the backend block session
	 *
	 * The CBE will transfer the payload to the given data.
	 *
	 * \param  request  reference to the Block::Request processed
	 *                  by the CBE
	 * \param  data     reference to the data associated with the
	 *                  Request
	 *
	 * \return  true if the CBE could process the request
	 */
	void io_request_in_progress(Io_buffer::Index const &data_index);

	/**
	 * Query list of active snapshots
	 *
	 * \param  ids  reference to destination buffer
	 */
	void active_snapshot_ids(Active_snapshot_ids &ids) const;

		bool librara__peek_generated_request(Genode::uint8_t *buf_ptr,
		                                     Genode::size_t   buf_size,
		                                     Io_buffer       &io_buf);

		void librara__drop_generated_request(void *prim_ptr);

		void librara__generated_request_complete(void *prim_ptr,
		                                         void *blk_data_ptr,
		                                         void *key_plain_ptr,
		                                         void *key_cipher_ptr,
		                                         void *hash_ptr,
		                                         bool  success);
};

#endif /* _CBE_LIBRARY_H_ */
