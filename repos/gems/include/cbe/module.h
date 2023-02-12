
#ifndef _MODULE_H_
#define _MODULE_H_

#include <base/log.h>

namespace Cbe {

	class Module_request
	{
		private:

			unsigned long _src_module_id   { ~0UL };
			unsigned long _src_request_id  { ~0UL };
			unsigned long _dst_module_id   { ~0UL };
			unsigned long _dst_request_id  { ~0UL };
			bool          _success         { false };

		public:

			Module_request() { }

			Module_request(unsigned long src_module_id,
			               unsigned long src_request_id,
			               unsigned long dst_module_id);

			unsigned long src_module_id() const { return _src_module_id; }
			unsigned long src_request_id() const { return _src_request_id; }
			unsigned long dst_module_id() const { return _dst_module_id; }
			unsigned long dst_request_id() const { return _dst_request_id; }
			bool success() const { return _success; }

			void dst_request_id(unsigned long id)
			{
				_dst_request_id = id;

				Genode::log(
					"Module_request::", __func__, " src ", _src_module_id, " src req ",
					_src_request_id, " dst ", _dst_module_id, " dst req ", _dst_request_id);
			}

			void success(bool succ)
			{
				_success = succ;

				Genode::log(
					"Module_request::", __func__, " src ", _src_module_id, " src req ",
					_src_request_id, " dst ", _dst_module_id, " dst req ", _dst_request_id, " success ", _success);
			}


			/*****************************************************
			 ** can be removed once the cbe translation is done **
			 *****************************************************/

			virtual void *prim()
			{
				class Bad_call { };
				throw Bad_call { };
			};

			virtual ~Module_request() { }
	};

	class Module
	{
		private:

			virtual bool _peek_completed_request(Genode::uint8_t *buf_ptr,
			                                     Genode::size_t   buf_size) = 0;

			virtual void _drop_completed_request(Module_request &req) = 0;

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
				Genode::uint8_t buf[300];
				while (_peek_generated_request(buf, sizeof(buf))) {

					Module_request &req = *(Module_request *)buf;
					Genode::log("Module::for_each_generated_request: from module ", req.src_module_id(), " to ", req.dst_module_id(), " handle");
					switch (handle_request(req)) {
					case Module::REQUEST_HANDLED:

						Genode::log("Module::for_each_generated_request: from module ", req.src_module_id(), " to ", req.dst_module_id(), " drop");
						_drop_generated_request(req);
						break;

					case Module::REQUEST_NOT_HANDLED:

						return;
					}
				}
			}

			virtual void generated_request_complete(Module_request &req) = 0;

			template <typename FUNC>
			void for_each_completed_request(FUNC && handle_request)
			{
				Genode::uint8_t buf[300];
				while (_peek_completed_request(buf, sizeof(buf))) {

					Module_request &req = *(Module_request *)buf;
					Genode::log("Module::for_each_completed_request: from module ", req.src_module_id(), " to ", req.dst_module_id(), " handle");
					handle_request(req);
					Genode::log("Module::for_each_completed_request: from module ", req.src_module_id(), " to ", req.dst_module_id(), " drop");
					_drop_completed_request(req);
				}
			}

			virtual ~Module() { }
	};
}

#endif /* _MODULE_H_ */
