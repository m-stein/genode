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
	public:

		Library();

		void execute();

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

		bool librara__peek_generated_request(Genode::uint8_t *buf_ptr,
		                                     Genode::size_t   buf_size);

		void librara__drop_generated_request(void *prim_ptr);

		void librara__generated_request_complete(void             *prim_ptr,
		                                         Superblock_state  sb_state,
		                                         bool              success);
};

#endif /* _CBE_LIBRARY_H_ */
