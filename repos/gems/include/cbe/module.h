
#ifndef _MODULE_H_
#define _MODULE_H_

#include <base/log.h>

namespace Cbe {

	class Module_request
	{
		private:

			bool          _valid         { false };
			unsigned long _dst_module_id { 0 };

		public:

			Module_request() { }

			Module_request(unsigned long dst_module_id);

			bool valid() const { return _valid; }

			unsigned long dst_module_id() const { return _dst_module_id; }


			/*****************************************************
			 ** can be removed once the cbe translation is done **
			 *****************************************************/

			virtual void *prim()
			{
				Genode::error(__func__, " ", __LINE__);
				throw -1;
				return nullptr;
			};

			virtual ~Module_request() { }
	};

	class Module
	{
		private:

			virtual bool _peek_generated_request(Genode::uint8_t *buf_ptr,
			                                     Genode::size_t   buf_size) = 0;

			virtual void _drop_generated_request(Module_request &req) = 0;

		public:

			enum Handle_request_result { REQUEST_HANDLED, REQUEST_NOT_HANDLED };

			typedef Handle_request_result (*Handle_request_function)(Module_request &req);

			virtual bool ready_to_submit_request() = 0;

			virtual void submit_request(Module_request &req) = 0;

			virtual void execute(bool &progress) = 0;

			template <typename FUNC>
			void for_each_generated_request(FUNC && handle_request)
			{
				Genode::uint8_t buf[256];
				while (_peek_generated_request(buf, sizeof(buf))) {

					Module_request &req = *(Module_request *)buf;
					if (!req.valid())
						return;

					switch (handle_request(req)) {
					case Module::REQUEST_HANDLED:

						_drop_generated_request(req);
						break;

					case Module::REQUEST_NOT_HANDLED:

						return;
					}
				}
			}

			virtual ~Module() { }
	};
}

#endif /* _MODULE_H_ */
