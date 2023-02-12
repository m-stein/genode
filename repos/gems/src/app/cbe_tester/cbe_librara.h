#ifndef _CBE_LIBRARA_H_
#define _CBE_LIBRARA_H_

/* base includes */
#include <util/reconstructible.h>

/* gems includes */
#include <cbe/library.h>
#include <cbe/module.h>

namespace Cbe {

	class Librara;
}


class Cbe::Librara : public Module
{
	private:

		Genode::Constructible<Library> &_lib;


		/************
		 ** Module **
		 ************/

		bool _peek_completed_request(Genode::uint8_t *,
		                             Genode::size_t   ) override
		{
			return false;
		}

		void _drop_completed_request(Module_request &) override
		{
			class Bad_call { };
			throw Bad_call { };
		}

		bool _peek_generated_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override
		{
			if (!_lib.constructed())
				return false;

			return _lib->librara__peek_generated_request(buf_ptr, buf_size);
		}

		void _drop_generated_request(Module_request &mod_req) override;

	public:

		Librara(Genode::Constructible<Library> &lib) : _lib { lib } { }


		/************
		 ** Module **
		 ************/

		void generated_request_complete(Module_request &) override;

		bool ready_to_submit_request() override
		{
			class Bad_call { };
			throw Bad_call { };
		};

		void submit_request(Module_request &) override
		{
			class Bad_call { };
			throw Bad_call { };
		}

		void execute(bool &) override
		{
		}
};

#endif /* _CBE_LIBRARA_H_ */
