/*
 * \brief  Framework for combining individual async request processing units
 * \author Martin Stein
 * \date   2020-08-26
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _REQUEST_PROCESSOR_H_
#define _REQUEST_PROCESSOR_H_

namespace Genode {

	template <typename      REQUEST,
	          typename      CHANNEL,
	          unsigned long NR_OF_CHANNELS>

	class Request_processor;
}


template <typename      REQUEST,
          typename      CHANNEL,
          unsigned long NR_OF_CHANNELS>

class Genode::Request_processor
{
	protected:

		CHANNEL  _channels[NR_OF_CHANNELS];
		CHANNEL *_unused_channel_ptr { _channels };

	public:

		Request_processor()
		{
			for (CHANNEL &channel : _channels)
				channel = CHANNEL { };
		}

		bool ready_to_submit_request()
		{
			return _unused_channel_ptr != nullptr;
		}

		void submit_request(REQUEST req)
		{
			if (_unused_channel_ptr == nullptr) {
				class Invalid_call_to_submit_request { };
				throw Invalid_call_to_submit_request { };
			}
			_unused_channel_ptr->use(req);
			_unused_channel_ptr = nullptr;

			for (CHANNEL &channel : _channels) {
				if (channel.unused()) {
					_unused_channel_ptr = &channel;
					break;
				}
			}
		}

		REQUEST const *peek_completed_request() const
		{
			for (CHANNEL const &channel : _channels) {
				if (channel.completed())
					return &channel.request;
			}
			return nullptr;
		}

		void drop_completed_request()
		{
			for (CHANNEL &channel : _channels) {
				if (channel.completed()) {
					channel = CHANNEL { };
					_unused_channel_ptr = &channel;
					return;
				}
			}
			class Invalid_call_to_drop_completed_request { };
			throw Invalid_call_to_drop_completed_request { };
		}

		REQUEST peek_generated_request() const
		{
			for (CHANNEL const &channel : _channels) {
				REQUEST req;
				if (channel.has_generated_request(req))
					return req;
			}
			return REQUEST { };
		}

		void drop_generated_request()
		{
			for (CHANNEL const &channel : _channels) {
				if (channel.drop_generated_request())
					return;
			}
			class Invalid_call_to_drop_generated_request { };
			throw Invalid_call_to_drop_generated_request { };
		}

		void generated_request_completed(REQUEST const &req)
		{
			for (CHANNEL const &channel : _channels) {
				if (channel.generated_request_completed(req))
					return;
			}
			class Invalid_call_to_generated_request_completed { };
			throw Invalid_call_to_generated_request_completed { };
		}
};

#endif /* _REQUEST_PROCESSOR_H_ */
