/*
 * Copyright (C) 2020 Genode Labs GmbH, Componolit GmbH, secunet AG
 *
 * This file is part of the Consistent Block Encrypter project, which is
 * distributed under the terms of the GNU Affero General Public License
 * version 3.
 */

#ifndef _CBE_EXTERNAL_TA_H_
#define _CBE_EXTERNAL_TA_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/output.h>

/* CBE includes */
#include <cbe/types.h>
#include <cbe/spark_object.h>


extern "C" void external_trust_anchor_cxx_init();
extern "C" void external_trust_anchor_cxx_final();


namespace External {

	using namespace Cbe;

	class Trust_anchor;

	Genode::uint32_t object_size(Trust_anchor const &);

} /* namespace Cbe */


class External::Trust_anchor : public Cbe::Spark_object<312>
{
	private:

		/*
		 * Ada/SPARK compatible bindings
		 *
		 * Ada functions cannot have out parameters. Hence we call Ada
		 * procedures that return the 'progress' result as last out parameter.
		 */

		void _execute(bool &);

	public:

	/**
	 * Constructor
	 */
	Trust_anchor();

	/**
	 * Check if the TA can accept a new requeust
	 *
	 * \return true if a request can be accepted, otherwise false
	 */
	bool request_acceptable() const;

	/**
	 * Submit a new create key request
	 *
	 * This method must only be called after executing 'request_acceptable'
	 * returned true.
	 *
	 * \param request  reference to valid TA create key request
	 */
	void submit_create_key_request(Trust_anchor_request const &request);

	/**
	 * Submit a new encrypt key request
	 *
	 * This method must only be called after executing 'request_acceptable'
	 * returned true.
	 *
	 * \param request  reference to valid TA encrypt key request
	 * \param key      reference to plaintext key value
	 */
	void submit_encrypt_key_request(Trust_anchor_request const &request,
	                                Key_plaintext_value  const &key);

	/**
	 * Submit a new decrypt key request
	 *
	 * This method must only be called after executing 'request_acceptable'
	 * returned true.
	 *
	 * \param request  reference to valid TA decrypt key request
	 * \param key      reference to ciphertext key value
	 */
	void submit_decrypt_key_request(Trust_anchor_request const &request,
	                                Key_ciphertext_value const &key);

	/**
	 * Submit a new secure superblock request
	 *
	 * This method must only be called after executing 'request_acceptable'
	 * returned true.
	 *
	 * \param request  reference to valid TA secure superblock request
	 * \param hash     reference to superblock hash
	 */
	void submit_secure_superblock_request(Trust_anchor_request const &request,
	                                      Hash                 const &hash);

	/**
	 * Check for any completed request
	 *
	 * \return a valid TA request will be returned if there is an
	 *         completed request, otherwise an invalid one
	 */
	Trust_anchor_request peek_completed_request() const;

	/**
	 * Get plaintext key value for completed request
	 *
	 * \param request  reference to valid TA request
	 *
	 * \return a valid TA request will be returned if there is an
	 *         completed request, otherwise an invalid one
	 */
	Key_plaintext_value peek_completed_key_value_plaintext(Trust_anchor_request const &request) const;

	/**
	 * Get ciphertext key value for completed request
	 *
	 * \param request  reference to valid TA request
	 *
	 * \return a valid TA request will be returned if there is an
	 *         completed request, otherwise an invalid one
	 */
	Key_ciphertext_value peek_completed_key_value_ciphertext(Trust_anchor_request const &request) const;

	/**
	 * Drop the completed request
	 *
	 * This method must only be called after executing
	 * 'peek_completed_request' returned a valid request.
	 *
	 */
	void drop_completed_request(Trust_anchor_request const &req);

	/**
	 * Execute TA operations
	 *
	 * \return true if progress was made, otherwise false is returned
	 */
	bool execute()
	{
		bool progress = false;
		_execute(progress);
		return progress;
	}
};

#endif /* _CBE_EXTERNAL_TA_H_ */
