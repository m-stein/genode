/*
 * \brief  Framework for component internal modularization
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODULE_H_
#define _MODULE_H_

/* base includes */
#include <util/string.h>
#include <base/log.h>

namespace Cbe {

	char const *module_name(unsigned long);

	class Module_request
	{
		private:

			unsigned long _src_module_id   { ~0UL };
			unsigned long _src_request_id  { ~0UL };
			unsigned long _dst_module_id   { ~0UL };
			unsigned long _dst_request_id  { ~0UL };

		public:

			Module_request() { }

			Module_request(unsigned long src_module_id,
			               unsigned long src_request_id,
			               unsigned long dst_module_id);

			unsigned long src_module_id() const { return _src_module_id; }
			unsigned long src_request_id() const { return _src_request_id; }
			unsigned long dst_module_id() const { return _dst_module_id; }
			unsigned long dst_request_id() const { return _dst_request_id; }

			void dst_request_id(unsigned long id) { _dst_request_id = id; }

			virtual char const *type_name() { return "?"; };

			Genode::String<32> src_request_id_str() const
			{
				return
					_src_request_id == ~0UL ?
						Genode::String<32> { "?" } :
						Genode::String<32> { _src_request_id };
			}

			Genode::String<32> dst_request_id_str() const
			{
				return
					_dst_request_id == ~0UL ?
						Genode::String<32> { "?" } :
						Genode::String<32> { _dst_request_id };
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
					switch (handle_request(req)) {
					case Module::REQUEST_HANDLED:

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
					handle_request(req);
					_drop_completed_request(req);
				}
			}

			virtual ~Module() { }
	};

	enum Module_id : unsigned long
	{
		CRYPTO      = 0,
		CBE_LIBRARA = 1,
		CLIENT_DATA = 2,
	};
}

#endif /* _MODULE_H_ */
