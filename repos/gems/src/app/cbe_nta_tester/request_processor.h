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

	template <typename      DERIVED_TYPE,
	          typename      REQUEST,
	          typename      CHANNEL,
	          unsigned long NR_OF_CHANNELS>

	class Request_processor;
}


template <typename      DERIVED_TYPE,
          typename      REQUEST,
          typename      CHANNEL,
          unsigned long NR_OF_CHANNELS>

class Genode::Request_processor
{
	protected:

		CHANNEL  _channels[NR_OF_CHANNELS];
		CHANNEL *_unused_channel_ptr { _channels };
		bool     _progress_possible  { false };

		DERIVED_TYPE &_derived_obj()
		{
			return *static_cast<DERIVED_TYPE *>(this);
		}

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
			_progress_possible = true;

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
					return &channel.request();
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

		void generated_request_completed(REQUEST const &req)
		{
			for (CHANNEL const &channel : _channels) {
				if (channel.generated_request_completed(req)) {
					_progress_possible = true;
					return;
				}
			}
			class Invalid_call_to_generated_request_completed { };
			throw Invalid_call_to_generated_request_completed { };
		}

		void execute(bool &progress)
		{
			if (!_progress_possible)
				return;

			while (true) {

				bool local_progress { false };
				_derived_obj().execute_one_step(local_progress);

				if (local_progress) {
					progress = true;
				} else {
					_progress_possible = false;
					return;
				}
			}
		}
};

#endif /* _REQUEST_PROCESSOR_H_ */
