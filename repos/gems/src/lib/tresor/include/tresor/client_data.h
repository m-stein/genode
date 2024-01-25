/*
 * \brief  Module that provides access to the client request data
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__CLIENT_DATA_H_
#define _TRESOR__CLIENT_DATA_H_

/* tresor includes */
#include <tresor/types.h>

namespace Tresor {

	class Client_datx;
	class Client_data_obtain;
	class Client_data_request;
}

namespace Vfs_tresor { class Client_data; }

namespace Tresor_tester { class Client_data; }

class Tresor::Client_data_obtain
{
	public:

		using Module = Client_datx;

		struct Attr
		{
			Request_offset const in_req_off;
			Request_tag const in_req_tag;
			Physical_block_address const in_pba;
			Virtual_block_address const in_vba;
			Block &out_blk;
		};

		struct Back_end : Interface
		{
			virtual void obtain_client_data(Attr const &attr) = 0;
		};

	private:

		enum State { INIT, COMPLETE };

		Request_helper<Client_data_obtain, State> _helper;

		NONCOPYABLE(Client_data_obtain);

	public:

		Client_data_obtain(Attr const &attr) : _helper(*this, attr) { }

		void print(Output &out) const { Genode::print(out, "obtain client data"); }

		bool execute(Back_end &back_end)
		{
			bool progress { false };
			switch(_helper.state) {
			case INIT:
				back_end.obtain_client_data(_helper.attr);
				_helper.mark_succeeded(progress);
				break;
			default: break;
			}
			return progress;
		}

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Client_datx : public Client_data_obtain::Back_end
{
	private:

		NONCOPYABLE(Client_datx);

	public:

		Client_datx() { }

		template <typename REQ>
		bool execute(REQ &req) { return req.execute(*this); }

		static constexpr char const *name() { return "client_data"; }
};

class Tresor::Client_data_request : public Module_request
{
	friend class ::Vfs_tresor::Client_data;
	friend class ::Tresor_tester::Client_data;

	public:

		enum Type { OBTAIN_PLAINTEXT_BLK, SUPPLY_PLAINTEXT_BLK };

	private:

		Type const _type;
		Request_offset const _req_off;
		Request_tag const _req_tag;
		Physical_block_address const _pba;
		Virtual_block_address const _vba;
		Block &_blk;
		bool &_success;

		NONCOPYABLE(Client_data_request);

	public:

		Client_data_request(Module_id src_mod_id, Module_channel_id src_chan_id, Type type,
		                    Request_offset req_off, Request_tag req_tag, Physical_block_address pba,
		                    Virtual_block_address vba, Block &blk, bool &success)
		:
			Module_request(src_mod_id, src_chan_id, CLIENT_DATA), _type(type), _req_off(req_off),
			_req_tag(req_tag), _pba(pba), _vba(vba), _blk(blk), _success(success)
		{ }

		static char const *type_to_string(Type type)
		{
			switch (type) {
			case OBTAIN_PLAINTEXT_BLK: return "obtain_plaintext_blk";
			case SUPPLY_PLAINTEXT_BLK: return "supply_plaintext_blk";
			}
			ASSERT_NEVER_REACHED;
		}

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};

#endif /* _TRESOR__CLIENT_DATA_H_ */
