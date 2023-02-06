
#ifndef _CRYPTO_H_
#define _CRYPTO_H_

/* gems includes */
#include <cbe/types.h>

/* cbe tester includes */
#include <module.h>
#include <vfs_utilities.h>

namespace Cbe
{
	class Crypto;
	class Crypto_request;
	class Crypto_channel;

	enum { KEY_SIZE = 32 };
	enum { PRIM_BUF_SIZE = 128 };
}

class Cbe::Crypto_request : public Module_request
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

		friend class Crypto;
		friend class Crypto_channel;

		Type             _type                    { INVALID };
		Genode::uint64_t _client_req_offset       { 0 };
		Genode::uint64_t _client_req_tag          { 0 };
		Genode::uint64_t _pba                     { 0 };
		Genode::uint64_t _vba                     { 0 };
		Genode::uint32_t _key_id                  { 0 };
		Genode::uint8_t  _prim[PRIM_BUF_SIZE]     { };
		Genode::uint8_t  _key_plaintext[KEY_SIZE] { };
		Genode::addr_t   _plaintext_blk_ptr       { 0 };
		Genode::addr_t   _ciphertext_blk_ptr      { 0 };
		bool             _success                 { false };

	public:

		char const *type_name() override
		{
			switch (_type) {
			case INVALID: return "invalid";
			case ADD_KEY: return "add_key";
			case REMOVE_KEY: return "remove_key";
			case ENCRYPT_CLIENT_DATA: return "encrypt_client_data";
			case DECRYPT_CLIENT_DATA: return "decrypt_client_data";
			case ENCRYPT: return "encrypt";
			case DECRYPT: return "decrypt";
			}
			return "?";
		}

		Crypto_request() { }

		Type type() const { return _type; }


		/*****************************************************
		 ** can be removed once the cbe translation is done **
		 *****************************************************/

		Crypto_request(unsigned long src_module_id,
		               unsigned long src_request_id)
		:
			Module_request { src_module_id, src_request_id, CRYPTO }
		{ }

		static void create(
			void             * buf_ptr,
			Genode::size_t     buf_size,
			Genode::size_t     req_type,
			Genode::uint64_t   client_req_offset,
			Genode::uint64_t   client_req_tag,
			void             * prim_ptr,
			size_t             prim_size,
			Genode::uint32_t   key_id,
			void             * key_plaintext_ptr,
			Genode::uint64_t   pba,
			Genode::uint64_t   vba,
			void             * plaintext_blk_ptr,
			void             * ciphertext_blk_ptr);

		void *prim() override { return (void *)&_prim; }

		void *result_blk_ptr()
		{
			switch (_type) {
			case DECRYPT: return (void *)_plaintext_blk_ptr;
			case ENCRYPT: return (void *)_ciphertext_blk_ptr;
			case INVALID:
			case ADD_KEY:
			case REMOVE_KEY:
			case DECRYPT_CLIENT_DATA:
			case ENCRYPT_CLIENT_DATA:
				break;
			}
			return nullptr;
		}

		bool success() const { return _success; }
};

class Cbe::Crypto_channel
{
	private:

		friend class Crypto;

		enum State {
			INACTIVE, SUBMITTED, COMPLETE, OBTAIN_PLAINTEXT_BLK_PENDING,
			OBTAIN_PLAINTEXT_BLK_IN_PROGRESS, OBTAIN_PLAINTEXT_BLK_COMPLETE,
			SUPPLY_PLAINTEXT_BLK_PENDING, SUPPLY_PLAINTEXT_BLK_IN_PROGRESS,
			SUPPLY_PLAINTEXT_BLK_COMPLETE, OP_WRITTEN_TO_VFS_HANDLE,
			QUEUE_READ_SUCCEEDED };

		State            _state                    { INACTIVE };
		Crypto_request   _request                  { };
		bool             _generated_req_success    { false };
		Vfs::Vfs_handle *_vfs_handle               { nullptr };
		char             _blk_buf[Cbe::BLOCK_SIZE] {  };

	public:

		Crypto_request const &request() const { return _request; }
};

class Cbe::Crypto : public Module
{
	private:

		using Request = Crypto_request;
		using Channel = Crypto_channel;
		using Write_result = Vfs::File_io_service::Write_result;
		using Read_result = Vfs::File_io_service::Read_result;

		enum { NR_OF_CHANNELS = 1 };

		struct Key_directory
		{
			Vfs::Vfs_handle  *encrypt_handle { nullptr };
			Vfs::Vfs_handle  *decrypt_handle { nullptr };
			Genode::uint32_t  key_id         { 0 };
		};

		Vfs::Env                 &_vfs_env;
		Genode::String<32> const  _path;
		Vfs::Vfs_handle          &_add_key_handle;
		Vfs::Vfs_handle          &_remove_key_handle;
		Channel                   _channels[NR_OF_CHANNELS];
		Key_directory             _key_dirs[2] { { }, { } };

		Key_directory &_lookup_key_dir(Genode::uint32_t key_id);

		void _execute_add_key(Channel &channel,
		                      bool    &progress);

		void _execute_remove_key(Channel &channel,
		                         bool    &progress);

		void _execute_decrypt(Channel &channel,
		                      bool    &progress);

		void _execute_encrypt(Channel &channel,
		                      bool    &progress);

		void _execute_encrypt_client_data(Channel &channel,
		                                  bool    &progress);

		void _execute_decrypt_client_data(Channel &channel,
		                                  bool    &progress);

		void _mark_req_failed(Channel    &channel,
		                      bool       &progress,
		                      char const *str);

		void _mark_req_successful(Channel &channel,
		                          bool    &progress);


		/************
		 ** Module **
		 ************/

		bool _peek_completed_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override
		{
			for (Channel &channel : _channels) {
				if (channel._state == Channel::COMPLETE) {
					if (sizeof(channel._request) > buf_size) {
						class Exception_1 { };
						throw Exception_1 { };
					}
					Genode::memcpy(buf_ptr, &channel._request,
					               sizeof(channel._request));;
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
				class Exception_1 { };
				throw Exception_1 { };
			}
			if (_channels[id]._state != Channel::COMPLETE) {
				class Exception_2 { };
				throw Exception_2 { };
			}
			_channels[id]._state = Channel::INACTIVE;
		}

		bool _peek_generated_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override;

		void _drop_generated_request(Module_request &mod_req) override;

	public:

		Crypto(Vfs::Env               &vfs_env,
		       Genode::Xml_node const &xml_node);


		/************
		 ** Module **
		 ************/

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
					_channels[id]._state = Channel::SUBMITTED;
					return;
				}
			}
			class Invalid_call { };
			throw Invalid_call { };
		}

		void execute(bool &) override;

		void generated_request_complete(Module_request &req) override;
};

#endif /* _CRYPTO_H_ */
