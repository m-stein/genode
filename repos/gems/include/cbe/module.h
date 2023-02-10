
#ifndef _MODULE_H_
#define _MODULE_H_

#include <base/log.h>

namespace Cbe {

	class Module_request
	{
		public:

			enum Handle_generated_request_result { HANDLED, NOT_HANDLED };

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
		public:

			virtual void execute(bool &progress) = 0;

			virtual ~Module() { }
	};
}

#endif /* _MODULE_H_ */
