
#ifndef _CRYPTA_H_
#define _CRYPTA_H_

namespace Cbe
{
	class Crypta;
	class Crypta_request;
	class Crypta_channel;

	enum { KEY_SIZE = 32 };
	enum { PRIM_BUF_SIZE = 128 };
}

class Cbe::Crypta_request
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
			ADD_KEY,

			/* from: SB Ctrl
			   args: key id */
			REMOVE_KEY,

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

		Type          _type       { INVALID };
		unsigned long _key_id     { 0 };
		unsigned long _data_idx   { 0 };
		unsigned long _block_addr { 0 };
		Request       _request    { };
		unsigned char _prim_buf[PRIM_BUF_SIZE];
		unsigned char _plaintext_key[KEY_SIZE];

	public:

		Crypta_request() { }

		Crypta_request(Type type)
		:
			_type { type }
		{ }

		Type type() const { return _type; }
};

class Cbe::Crypta_channel
{
	private:

		friend class Crypta;

		enum State { INACTIVE, IN_PROGRESS, COMPLETED };

		State          _state   { INACTIVE };
		Crypta_request _request { };

	public:

		Crypta_request const &request() const { return _request; }
};

class Cbe::Crypta
{
	private:

		using Request = Crypta_request;
		using Channel = Crypta_channel;

		Channel _channels[4];

	public:

		bool ready_to_submit_request()
		{
			log(__func__, " ", __LINE__); while(1);
			for (Channel &channel : _channels) {
				if (channel._state == Channel::INACTIVE)
					return true;
			}
			return false;
		}

		void submit_request(Request &request)
		{
			log(__func__, " ", __LINE__); while(1);
			for (Channel &channel : _channels) {
				if (channel._state == Channel::INACTIVE) {
					channel._request = request;
					channel._state = Channel::IN_PROGRESS;
					return;
				}
			}
			throw -1;
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

		void execute(bool &/*progress*/)
		{
			log(__func__, " ", __LINE__); while(1);
/*
			for (Channel &channel : _channels) {
				if (channel._state != INVALID) {
					switch (channel._request._type) {
					case:
			
					}
				}
			}
*/
		}

		template <typename FUNC>
		void with_generated_request(FUNC && functor) const
		{
			log(__func__, " ", __LINE__); while(1);
		}

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
