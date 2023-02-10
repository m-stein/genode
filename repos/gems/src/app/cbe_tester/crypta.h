
#ifndef _CRYPTA_H_
#define _CRYPTA_H_

/* gems includes */
#include <cbe/types.h>
#include <cbe/module.h>

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
			INVALID,

			/* from: VBD Rekeying
			   args: key id, cipher data idx */
			DECRYPT,

			/* from: VBD Rekeying
			   args: key id, plaintext data idx */
			ENCRYPT,

			/* from: SB Ctrl
			   args: plaintext key */
			ADD_KEY = 1,

			/* from: SB Ctrl
			   args: key id */
			REMOVE_KEY = 2,

			/* from: Blk IO
			   args: req, vba, key id, cipher data index */
			DECRYPT_AND_SUPPLY_CLIENT_DATA,

			/* from: Blk IO
			   args: req, vba, key id, plaintext data index */
			OBTAIN_AND_ENCRYPT_CLIENT_DATA,
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

	public:

		Crypta_request() { }

		Type type() const { return _type; }


		/*****************************************************
		 ** can be removed once the cbe translation is done **
		 *****************************************************/

		Crypta_request(unsigned long dst_module_id)
		:
			Module_request { dst_module_id }
		{ }

		static void create(
			void   *buf_ptr,
			size_t  buf_size,
			size_t  req_type,
			void   *prim_ptr,
			size_t  prim_size,
			void   *key_id_ptr,
			size_t  key_id_size,
			void   *key_plaintext_ptr,
			size_t  key_plaintext_size);

		void *prim() override { return (void *)&_prim; }
};

class Cbe::Crypta_channel
{
	private:

		friend class Crypta;

		enum State { INACTIVE, PENDING, IN_PROGRESS, COMPLETED };

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

		bool _peek_generated_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override
		{
			for (Genode::uint32_t idx { 0 }; idx < NR_OF_CHANNELS; idx++) {
				Channel &channel { _channels[idx] };
				if (channel._state == Channel::PENDING) {

					switch (channel._request._type) {
					case Request::ADD_KEY:
					{
						Key key;
						if (sizeof(key.value) != sizeof(channel._request._key_plaintext)) {
							class Bad_size_1 { };
							throw Bad_size_1 { };
						}
						Genode::memcpy(key.value, channel._request._key_plaintext, sizeof(key.value));
						key.id.value = channel._request._key_id;
						Cbe::Request cbe_req { Cbe::Request::Operation::READ, false, 0, 0, 1, 0, idx };
						Crypto_request req { cbe_req, key };

						if (sizeof(req) > buf_size) {
							class Bad_size_2 { };
							throw Bad_size_2 { };
						}
						Genode::memcpy(buf_ptr, &req, sizeof(req));;
						return true;
					}
					default:
						class Bad_type { };
						throw Bad_type { };
					}
				}
			}
			return false;
		}

		void _drop_generated_request(Module_request &) override
		{
			for (Channel const &channel : _channels) {
				if (channel._state == Channel::PENDING) {

					switch (channel._request._type) {
					case Request::ADD_KEY:
					{
						class Drop { };
						throw Drop { };
					}
					default:
						class Bad_type { };
						throw Bad_type { };
					}
				}
			}
		}

	public:

		bool ready_to_submit_request() override
		{
			for (Channel &channel : _channels) {
				if (channel._state == Channel::INACTIVE)
					return true;
			}
			return false;
		}

		void submit_request(Module_request &mod_request) override
		{
			for (Channel &channel : _channels) {
				if (channel._state == Channel::INACTIVE) {
					channel._request = *dynamic_cast<Request *>(&mod_request);
					channel._state = Channel::PENDING;
					log("Crypta::", __func__, ": type ", (int)channel._request._type, " key id ", channel._request._key_id);
					return;
				}
			}
			class Invalid_call { };
			throw Invalid_call { };
		}

		template <typename FUNC>
		void with_completed_request(FUNC && functor) const
		{
			log(__func__, " ", __LINE__); while(1);
			for (Channel &channel : _channels) {
				if (channel._state == Channel::COMPLETED) {
					functor(channel._request);
					return;
				}
			}
		}

		void execute(bool &/*progress*/) override
		{
			for (Channel &channel : _channels) {
				if (channel._state != Channel::INACTIVE) {
					switch (channel._request._type) {
					case Request::REMOVE_KEY:
					case Request::ADD_KEY:
						break;
					default:
						class Bad_request_type { };
						throw Bad_request_type { };
					}
				}
			}
		}

/*
		void for_each_generated_request(
			Handle_request_result (*handle_request) (Module_request &req)) override
		{
			error("Hallo ", handle_request);
		}

		template <typename FUNC>
		void for_each_generated_request(FUNC && functor) const override
		{
			for (unsigned long idx { 0 }; idx < NR_OF_CHANNELS; idx++) {
				Channel &channel { _channels[idx] };
				if (channel._state == PENDING) {
					Request req {
						Request::READ, false, 0, 0, 1, 0, idx };
				}
			}
		}
*/

		void generated_request_completed(unsigned long  /*dst_id*/,
		                                 void          * /*req_ptr*/)
		{
			log(__func__, " ", __LINE__); while(1);
			throw -1;
		}

		Crypta()
		{
			for (Channel &channel : _channels)
				channel = Channel { };
		}
};

#endif /* _CRYPTA_H_ */
