
#ifndef _CLIENT_DATA_H_
#define _CLIENT_DATA_H_

/* gems includes */
#include <cbe/types.h>

/* cbe tester includes */
#include <module.h>

namespace Cbe
{
	class Client_data_request;
}

class Main;

class Cbe::Client_data_request : public Module_request
{
	public:

		enum Type { INVALID, OBTAIN_PLAINTEXT_BLK, SUPPLY_PLAINTEXT_BLK };

	private:

		friend class ::Main;

		Type             _type              { INVALID };
		Genode::uint64_t _client_req_offset { 0 };
		Genode::uint64_t _client_req_tag    { 0 };
		Genode::uint64_t _pba               { 0 };
		Genode::uint64_t _vba               { 0 };
		Genode::addr_t   _plaintext_blk_ptr { 0 };
		bool             _success           { false };

	public:

		char const *type_name() override
		{
			switch (_type) {
			case INVALID: return "invalid";
			case OBTAIN_PLAINTEXT_BLK: return "obtain_plaintext_blk";
			case SUPPLY_PLAINTEXT_BLK: return "supply_plaintext_blk";
			}
			return "?";
		}

		Client_data_request() { }

		Type type() const { return _type; }


		/*****************************************************
		 ** can be removed once the cbe translation is done **
		 *****************************************************/

		Client_data_request(unsigned long    src_module_id,
		                    unsigned long    src_request_id,
		                    Type             type,
		                    Genode::uint64_t client_req_offset,
		                    Genode::uint64_t client_req_tag,
		                    Genode::uint64_t pba,
		                    Genode::uint64_t vba,
		                    Genode::addr_t   plaintext_blk_ptr)
		:
			Module_request     { src_module_id, src_request_id, CLIENT_DATA },
			_type              { type },
			_client_req_offset { client_req_offset },
			_client_req_tag    { client_req_tag },
			_pba               { pba },
			_vba               { vba },
			_plaintext_blk_ptr { plaintext_blk_ptr }
		{ }

		bool success() const { return _success; }
};

#endif /* _CLIENT_DATA_H_ */
