/*
 * Copyright (C) 2019 Genode Labs GmbH, Componolit GmbH, secunet AG
 *
 * This file is part of the Consistent Block Encrypter project, which is
 * distributed under the terms of the GNU Affero General Public License
 * version 3.
 */

/* Genode includes */
#include <base/component.h>

/* CBE includes */
#include <cbe/external_crypto.h>

/* repo includes */
#include <util/sha256_4k.h>


struct Main
{
	unsigned const key_id { 0x1234 };
	External::Crypto::Key_data key { };
	External::Crypto _crypto { };

	Cbe::Block_data _input { };
	Cbe::Block_data _output { };

	Main(Genode::Env &env)
	{
		Genode::memset(&_input, 0xa5, sizeof (Cbe::Block_data));
		Genode::memset(&_output, 0, sizeof (Cbe::Block_data));

		Sha256_4k::Hash input_a5_hash { };
		{
			Sha256_4k::Data const &data = *reinterpret_cast<Sha256_4k::Data const*>(&_input);
			Sha256_4k::hash(data, input_a5_hash);
		}

		Cbe::Request req { };
		req.block_number = 666;

		Genode::memset(key.value, 0, sizeof (External::Crypto::Key_data));
		Genode::memcpy(key.value, "Foobar", 6);

		_crypto.add_key(key_id, key);

		/* first encrypt */
		{
			if (!_crypto.encryption_request_acceptable()) {
				struct Encryption_request_denied : Genode::Exception { };
				throw Encryption_request_denied();
			}

			req.operation = Cbe::Request::Operation::WRITE;
			_crypto.submit_encryption_request(req, _input, key_id);

			if (!_crypto.execute()) {
				struct No_progress : Genode::Exception { };
				throw No_progress();
			}

			Cbe::Request const completed_req = _crypto.peek_completed_encryption_request();
			if (!completed_req.valid()) {
				struct Encryption_not_completed : Genode::Exception { };
				throw Encryption_not_completed();
			}

			if (!_crypto.supply_cipher_data(completed_req, _output)) {
				struct Could_not_supply_cipher_data : Genode::Exception { };
				throw Could_not_supply_cipher_data();
			}
		}

		/* then decryption */
		{
			if (!_crypto.decryption_request_acceptable()) {
				struct Encryption_request_denied : Genode::Exception { };
				throw Encryption_request_denied();
			}

			req.operation = Cbe::Request::Operation::READ;
			_crypto.submit_decryption_request(req, _output, key_id);

			if (!_crypto.execute()) {
				struct No_progress : Genode::Exception { };
				throw No_progress();
			}

			Cbe::Request const completed_req = _crypto.peek_completed_decryption_request();
			if (!completed_req.valid()) {
				struct Encryption_not_completed : Genode::Exception { };
				throw Encryption_not_completed();
			}

			if (!_crypto.supply_plain_data(completed_req, _input)) {
				struct Could_not_supply_cipher_data : Genode::Exception { };
				throw Could_not_supply_cipher_data();
			}
		}

		/* and then check result */
		Sha256_4k::Hash check_input_a5_hash { };
		{
			Sha256_4k::Data const &data = *reinterpret_cast<Sha256_4k::Data const*>(&_input);
			Sha256_4k::hash(data, check_input_a5_hash);
		}

		bool const match = Genode::memcmp(input_a5_hash.values, check_input_a5_hash.values,
		                                  sizeof (Sha256_4k::Hash)) == 0;
		if (!match) {
			struct Encryption_and_decryption_no_match : Genode::Exception { };
			throw Encryption_and_decryption_no_match();
		}

		env.parent().exit(0);
	}
};


extern "C" void adainit();


void Component::construct(Genode::Env &env)
{
	env.exec_static_constructors();

	/**
	 * We have to call adainit, so, the secondary stack of SPARK
	 * for, e.g., variable-sized return values gets initialized.
	 */
	adainit();

	Cbe::assert_valid_object_size<External::Crypto>();

	static Main m { env };
}
