/*
 * \brief  Tool for running tests and benchmarks on the CBE
 * \author Martin Stein
 * \date   2020-08-26
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <timer_session/connection.h>
#include <block_session/connection.h>
#include <vfs/simple_env.h>

/* CBE includes */
#include <cbe/dump/configuration.h>
#include <cbe/init/configuration.h>

/* CBE tester includes */
#include <crypto.h>
#include <trust_anchor.h>
#include <verbose_node.h>
#include <client_data.h>
#include <block_io.h>
#include <meta_tree.h>
#include <free_tree.h>
#include <request_pool.h>
#include <block_allocator.h>
#include <vbd_initializer.h>
#include <ft_initializer.h>
#include <sb_initializer.h>
#include <virtual_block_device.h>
#include <superblock_control.h>

using namespace Genode;
using namespace Cbe;
using namespace Vfs;

enum { VERBOSE_MODULE_COMMUNICATION = 0 };

namespace Cbe {

	char const *module_name(unsigned long id)
	{
		switch (id) {
		case CRYPTO: return "crypto";
		case BLOCK_IO: return "block_io";
		case CACHE: return "cache";
		case META_TREE: return "meta_tree";
		case FREE_TREE: return "free_tree";
		case VIRTUAL_BLOCK_DEVICE: return "vbd";
		case SUPERBLOCK_CONTROL: return "sb_control";
		case CLIENT_DATA: return "client_data";
		case TRUST_ANCHOR: return "trust_anchor";
		case COMMAND_POOL: return "command_pool";
		case BLOCK_ALLOCATOR: return "block_allocator";
		case VBD_INITIALIZER: return "vbd_initializer";
		case FT_INITIALIZER: return "ft_initializer";
		case SB_INITIALIZER: return "sb_initializer";
		case REQUEST_POOL: return "request_pool";
		default: break;
		}
		return "?";
	}
}


template <typename T>
T read_attribute(Xml_node const &node,
                 char     const *attr)
{
	T value { };

	if (!node.has_attribute(attr)) {

		error("<", node.type(), "> node misses attribute '", attr, "'");
		class Attribute_missing { };
		throw Attribute_missing { };
	}
	if (!node.attribute(attr).value(value)) {

		error("<", node.type(), "> node has malformed '", attr, "' attribute");
		class Malformed_attribute { };
		throw Malformed_attribute { };
	}
	return value;
}


static void print_blk_data(Block_data const &blk_data)
{
	for(unsigned long idx = 0; idx < Cbe::BLOCK_SIZE; idx += 64) {
		log(
			"  ", idx, ": ",
			Hex(blk_data.values[idx + 0], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 1], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 2], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 3], Hex::OMIT_PREFIX, Hex::PAD), " ",
			Hex(blk_data.values[idx + 4], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 5], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 6], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 7], Hex::OMIT_PREFIX, Hex::PAD), " ",
			Hex(blk_data.values[idx + 8], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 9], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 10], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 11], Hex::OMIT_PREFIX, Hex::PAD), " ",
			Hex(blk_data.values[idx + 12], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 13], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 14], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 15], Hex::OMIT_PREFIX, Hex::PAD), " ",
			Hex(blk_data.values[idx + 16], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 17], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 18], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 19], Hex::OMIT_PREFIX, Hex::PAD), " ",
			Hex(blk_data.values[idx + 20], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 21], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 22], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 23], Hex::OMIT_PREFIX, Hex::PAD), " ",
			Hex(blk_data.values[idx + 24], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 25], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 26], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 27], Hex::OMIT_PREFIX, Hex::PAD), " ",
			Hex(blk_data.values[idx + 28], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 29], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 30], Hex::OMIT_PREFIX, Hex::PAD),
			Hex(blk_data.values[idx + 31], Hex::OMIT_PREFIX, Hex::PAD));
	}
}


class Log_node
{
	private:

		String<128> const _string;

	public:

		Log_node(Xml_node const &node)
		:
			_string { node.attribute_value("string", String<128> { }) }
		{ }

		String<128> const &string() const { return _string; }

		void print(Genode::Output &out) const
		{
			Genode::print(out, "string=\"", _string, "\"");
		}
};


class Benchmark_node
{
	public:

		using Label = String<128>;

		enum Operation { START, STOP };

	private:

		Operation const _op;
		bool      const _label_avail;
		Label     const _label;

		Operation _read_op_attr(Xml_node const &node)
		{
			class Attribute_missing { };
			if (!node.has_attribute("op")) {
				throw Attribute_missing { };
			}
			if (node.attribute("op").has_value("start")) {
				return Operation::START;
			}
			if (node.attribute("op").has_value("stop")) {
				return Operation::STOP;
			}
			class Malformed_attribute { };
			throw Malformed_attribute { };
		}

		static char const *_op_to_string(Operation op)
		{
			switch (op) {
			case START: return "start";
			case STOP: return "stop";
			}
			return "?";
		}

	public:

		bool has_attr_label() const
		{
			return _op == Operation::START;
		}

		Benchmark_node(Xml_node const &node)
		:
			_op          { _read_op_attr(node) },
			_label_avail { has_attr_label() && node.has_attribute("label") },
			_label       { _label_avail ?
			               node.attribute_value("label", Label { }) :
			               Label { } }
		{ }

		Operation op() const { return _op; }
		bool label_avail() const { return _label_avail; }
		Label const &label() const { return _label; }

		void print(Genode::Output &out) const
		{
			Genode::print(out, "op=", _op_to_string(_op));
			if (_label_avail) {
				Genode::print(out, " label=", _label);
			}
		}
};


class Benchmark
{
	private:

		enum State { STARTED, STOPPED };

		Genode::Env                   &_env;
		Timer::Connection              _timer                   { _env };
		State                          _state                   { STOPPED };
		Microseconds                   _start_time              { 0 };
		uint64_t                       _nr_of_virt_blks_read    { 0 };
		uint64_t                       _nr_of_virt_blks_written { 0 };
		Constructible<Benchmark_node>  _start_node              { };
		uint64_t                       _id                      { 0 };

	public:

		Benchmark(Genode::Env &env) : _env { env } { }

		void submit_request(Benchmark_node const &node)
		{
			switch (node.op()) {
			case Benchmark_node::START:

				if (_state != STOPPED) {
					class Bad_state_to_start { };
					throw Bad_state_to_start { };
				}
				_id++;
				_nr_of_virt_blks_read = 0;
				_nr_of_virt_blks_written = 0;
				_state = STARTED;
				_start_node.construct(node);
				_start_time = _timer.curr_time().trunc_to_plain_us();
				break;

			case Benchmark_node::STOP:

				if (_state != STARTED) {
					class Bad_state_to_stop { };
					throw Bad_state_to_stop { };
				}
				uint64_t const stop_time_us {
					_timer.curr_time().trunc_to_plain_us().value };

				log("");
				if (_start_node->label_avail()) {
					log("Benchmark result \"", _start_node->label(), "\"");
				} else {
					log("Benchmark result (command ID ", _id, ")");
				}

				double const passed_time_sec {
					(double)(stop_time_us - _start_time.value) /
					(double)(1000 * 1000) };

				log("   Ran ", passed_time_sec, " seconds.");

				if (_nr_of_virt_blks_read != 0) {

					uint64_t const bytes_read {
						_nr_of_virt_blks_read * Cbe::BLOCK_SIZE };

					double const mibyte_read {
						(double)bytes_read / (double)(1024 * 1024) };

					double const mibyte_per_sec_read {
						(double)bytes_read / (double)passed_time_sec /
						(double)(1024 * 1024) };

					log("   Have read ", mibyte_read, " mebibyte in total.");
					log("   Have read ", mibyte_per_sec_read, " mebibyte per second.");
				}

				if (_nr_of_virt_blks_written != 0) {

					uint64_t bytes_written {
						_nr_of_virt_blks_written * Cbe::BLOCK_SIZE };

					double const mibyte_written {
						(double)bytes_written / (double)(1024 * 1024) };

					double const mibyte_per_sec_written {
						(double)bytes_written / (double)passed_time_sec /
						(double)(1024 * 1024) };

					log("   Have written ", mibyte_written, " mebibyte in total.");
					log("   Have written ", mibyte_per_sec_written, " mebibyte per second.");
				}
				log("");
				_state = STOPPED;
				break;
			}
		}

		void raise_nr_of_virt_blks_read()    { _nr_of_virt_blks_read++;    }
		void raise_nr_of_virt_blks_written() { _nr_of_virt_blks_written++; }
};


class Trust_anchor_node
{
	private:

		using Operation = Trust_anchor_request::Type;

		Operation  const _op;
		String<64> const _passphrase;

		Operation _read_op_attr(Xml_node const &node)
		{
			class Attribute_missing { };
			if (!node.has_attribute("op")) {
				throw Attribute_missing { };
			}
			if (node.attribute("op").has_value("initialize")) {
				return Operation::INITIALIZE;
			}
			class Malformed_attribute { };
			throw Malformed_attribute { };
		}

	public:

		Trust_anchor_node(Xml_node const &node)
		:
			_op               { _read_op_attr(node) },
			_passphrase       { has_attr_passphrase() ?
			                    node.attribute_value("passphrase", String<64>()) :
			                    String<64>() }
		{ }

		Operation         op()         const { return _op; }
		String<64> const &passphrase() const { return _passphrase; }

		bool has_attr_passphrase() const
		{
			return _op == Operation::INITIALIZE;
		}

		void print(Genode::Output &out) const
		{
			Genode::print(out, "op=",
				Trust_anchor_request::type_to_string(_op));

			if (has_attr_passphrase()) {
				Genode::print(out, " passphrase=", _passphrase);
			}
		}
};


class Request_node
{
	private:

		using Operation = Cbe::Request::Operation;

		Operation             const _op;
		Virtual_block_address const _vba;
		Number_of_blocks_old  const _count;
		bool                  const _sync;
		bool                  const _salt_avail;
		uint64_t              const _salt;

		Operation _read_op_attr(Xml_node const &node)
		{
			class Attribute_missing { };
			if (!node.has_attribute("op")) {
				throw Attribute_missing { };
			}
			if (node.attribute("op").has_value("read")) {
				return Operation::READ;
			}
			if (node.attribute("op").has_value("write")) {
				return Operation::WRITE;
			}
			if (node.attribute("op").has_value("sync")) {
				return Operation::SYNC;
			}
			if (node.attribute("op").has_value("create_snapshot")) {
				return Operation::CREATE_SNAPSHOT;
			}
			if (node.attribute("op").has_value("extend_ft")) {
				return Operation::EXTEND_FT;
			}
			if (node.attribute("op").has_value("extend_vbd")) {
				return Operation::EXTEND_VBD;
			}
			if (node.attribute("op").has_value("rekey")) {
				return Operation::REKEY;
			}
			if (node.attribute("op").has_value("deinitialize")) {
				return Operation::DEINITIALIZE;
			}
			class Malformed_attribute { };
			throw Malformed_attribute { };
		}

	public:

		Request_node(Xml_node const &node)
		:
			_op         { _read_op_attr(node) },
			_vba        { has_attr_vba() ?
			              read_attribute<uint64_t>(node, "vba") : 0 },
			_count      { has_attr_count() ?
			              read_attribute<uint64_t>(node, "count") : 0 },
			_sync       { read_attribute<bool>(node, "sync") },
			_salt_avail { has_attr_salt() ?
			              node.has_attribute("salt") : false },
			_salt       { has_attr_salt() && _salt_avail ?
			              read_attribute<uint64_t>(node, "salt") : 0 }
		{ }

		Operation               op()         const { return _op; }
		Virtual_block_address   vba()        const { return _vba; }
		Number_of_blocks_old    count()      const { return _count; }
		bool                    sync()       const { return _sync; }
		bool                    salt_avail() const { return _salt_avail; }
		uint64_t                salt()       const { return _salt; }

		bool has_attr_vba() const
		{
			return _op == Operation::READ ||
			       _op == Operation::WRITE ||
			       _op == Operation::SYNC;
		}

		bool has_attr_salt() const
		{
			return _op == Operation::READ ||
			       _op == Operation::WRITE;
		}

		bool has_attr_count() const
		{
			return _op == Operation::READ ||
			       _op == Operation::WRITE ||
			       _op == Operation::SYNC ||
			       _op == Operation::EXTEND_FT ||
			       _op == Operation::EXTEND_VBD;
		}

		void print(Genode::Output &out) const
		{
			Genode::print(out, "op=", to_string(_op));
			if (has_attr_vba()) {
				Genode::print(out, " vba=", _vba);
			}
			if (has_attr_count()) {
				Genode::print(out, " count=", _count);
			}
			Genode::print(out, " sync=", _sync);
			if (_salt_avail) {
				Genode::print(out, " salt=", _salt);
			}
		}
};


class Command : public Fifo<Command>::Element
{
	public:

		enum Type
		{
			INVALID,
			REQUEST,
			TRUST_ANCHOR,
			BENCHMARK,
			CONSTRUCT,
			DESTRUCT,
			INITIALIZE,
			CHECK,
			DUMP,
			LIST_SNAPSHOTS,
			LOG
		};

		enum State
		{
			PENDING,
			IN_PROGRESS,
			COMPLETED
		};

	private:

		Type                                   _type              { INVALID };
		uint32_t                               _id                { 0 };
		State                                  _state             { PENDING };
		bool                                   _success           { false };
		bool                                   _data_mismatch     { false };
		Constructible<Request_node>            _request_node      { };
		Constructible<Trust_anchor_node>       _trust_anchor_node { };
		Constructible<Benchmark_node>          _benchmark_node    { };
		Constructible<Log_node>                _log_node          { };
		Constructible<Cbe_init::Configuration> _initialize        { };
		Constructible<Cbe_dump::Configuration> _dump              { };

		char const *_state_to_string() const
		{
			switch (_state) {
			case PENDING: return "pending";
			case IN_PROGRESS: return "in_progress";
			case COMPLETED: return "completed";
			}
			return "?";
		}

		char const *_type_to_string() const
		{
			switch (_type) {
			case INITIALIZE: return "initialize";
			case INVALID: return "invalid";
			case DUMP: return "dump";
			case REQUEST: return "request";
			case TRUST_ANCHOR: return "trust_anchor";
			case BENCHMARK: return "benchmark";
			case CONSTRUCT: return "construct";
			case DESTRUCT: return "destruct";
			case CHECK: return "check";
			case LIST_SNAPSHOTS: return "list_snapshots";
			case LOG: return "log";
			}
			return "?";
		}

	public:

		Command() { }

		Command(Type            type,
		        Xml_node const &node,
		        uint32_t        id)
		:
			_type { type },
			_id   { id }
		{
			switch (_type) {
			case INITIALIZE:   _initialize.construct(node);        break;
			case DUMP:         _dump.construct(node);              break;
			case REQUEST:      _request_node.construct(node);      break;
			case TRUST_ANCHOR: _trust_anchor_node.construct(node); break;
			case BENCHMARK:    _benchmark_node.construct(node);    break;
			case LOG:          _log_node.construct(node);          break;
			default:                                               break;
			}
		}

		Command(Command &other)
		:
			_type    { other._type },
			_id      { other._id },
			_state   { other._state },
			_success { other._success }
		{
			switch (_type) {
			case INITIALIZE:   _initialize.construct(*other._initialize);               break;
			case DUMP:         _dump.construct(*other._dump);                           break;
			case REQUEST:      _request_node.construct(*other._request_node);           break;
			case TRUST_ANCHOR: _trust_anchor_node.construct(*other._trust_anchor_node); break;
			case BENCHMARK:    _benchmark_node.construct(*other._benchmark_node);       break;
			case LOG:          _log_node.construct(*other._log_node);                   break;
			default:                                                                    break;
			}
		}

		bool has_attr_data_mismatch() const
		{
			return
				_type == REQUEST &&
				_request_node->op() == Cbe::Request::Operation::READ &&
				_request_node->salt_avail();
		}

		bool synchronize() const
		{
			class Bad_type { };
			switch (_type) {
			case INITIALIZE:     return true;
			case BENCHMARK:      return true;
			case CONSTRUCT:      return true;
			case DESTRUCT:       return true;
			case DUMP:           return true;
			case CHECK:          return true;
			case TRUST_ANCHOR:   return true;
			case LIST_SNAPSHOTS: return true;
			case LOG:            return true;
			case REQUEST:        return _request_node->sync();
			case INVALID:        throw Bad_type { };
			}
			throw Bad_type { };
		}

		static Type type_from_string(String<64> str)
		{
			if (str == "initialize")     { return INITIALIZE; }
			if (str == "request")        { return REQUEST; }
			if (str == "trust-anchor")   { return TRUST_ANCHOR; }
			if (str == "benchmark")      { return BENCHMARK; }
			if (str == "construct")      { return CONSTRUCT; }
			if (str == "destruct")       { return DESTRUCT; }
			if (str == "check")          { return CHECK; }
			if (str == "dump")           { return DUMP; }
			if (str == "list-snapshots") { return LIST_SNAPSHOTS; }
			if (str == "log")            { return LOG; }
			class Bad_string { };
			throw Bad_string { };
		}

		void print(Genode::Output &out) const
		{
			Genode::print(out, "id=", _id, " type=", _type_to_string());
			class Bad_type { };
			switch (_type) {
			case INITIALIZE:     Genode::print(out, " cfg=(", *_initialize, ")"); break;
			case REQUEST:        Genode::print(out, " cfg=(", *_request_node, ")"); break;
			case TRUST_ANCHOR:   Genode::print(out, " cfg=(", *_trust_anchor_node, ")"); break;
			case BENCHMARK:      Genode::print(out, " cfg=(", *_benchmark_node, ")"); break;
			case DUMP:           Genode::print(out, " cfg=(", *_dump, ")"); break;
			case LOG:            Genode::print(out, " cfg=(", *_log_node, ")"); break;
			case INVALID:        break;
			case CHECK:          break;
			case CONSTRUCT:      break;
			case DESTRUCT:       break;
			case LIST_SNAPSHOTS: break;
			}
			Genode::print(out, " succ=", _success);
			if (has_attr_data_mismatch()) {
				Genode::print(out, " bad_data=", _data_mismatch);
			}
			Genode::print(out, " state=", _state_to_string());
		}

		Type                           type              () const { return _type              ; }
		State                          state             () const { return _state             ; }
		uint32_t                       id                () const { return _id                ; }
		bool                           success           () const { return _success           ; }
		bool                           data_mismatch     () const { return _data_mismatch     ; }
		Request_node            const &request_node      () const { return *_request_node     ; }
		Trust_anchor_node       const &trust_anchor_node () const { return *_trust_anchor_node; }
		Benchmark_node          const &benchmark_node    () const { return *_benchmark_node   ; }
		Log_node                const &log_node          () const { return *_log_node         ; }
		Cbe_init::Configuration const &initialize        () const { return *_initialize       ; }
		Cbe_dump::Configuration const &dump              () const { return *_dump             ; }

		void state         (State state)        { _state = state; }
		void success       (bool success)       { _success = success; }
		void data_mismatch (bool data_mismatch) { _data_mismatch = data_mismatch; }
};


class Command_pool : public Module {

	private:

		Allocator          &_alloc;
		Verbose_node const &_verbose_node;
		Fifo<Command>       _cmd_queue              { };
		uint32_t            _next_command_id        { 0 };
		unsigned long       _nr_of_uncompleted_cmds { 0 };
		unsigned long       _nr_of_errors           { 0 };
		Block_data          _blk_data               { };

		void _read_cmd_node(Xml_node const &node,
		                    Command::Type   cmd_type)
		{
			Command &cmd {
				*new (_alloc) Command(cmd_type, node, _next_command_id++) };

			_nr_of_uncompleted_cmds++;
			_cmd_queue.enqueue(cmd);

			if (_verbose_node.cmd_pool_cmd_pending()) {
				log("cmd pending: ", cmd);
			}
		}

		static void _generate_blk_data(Block_data            &blk_data,
		                               Virtual_block_address  vba,
		                               uint64_t               salt)
		{
			for (uint64_t idx { 0 };
			     idx + sizeof(vba) + sizeof(salt) <=
			        sizeof(blk_data.values) / sizeof(blk_data.values[0]); )
			{
				memcpy(&blk_data.values[idx], &vba, sizeof(vba));
				idx += sizeof(vba);
				memcpy(&blk_data.values[idx], &salt, sizeof(salt));
				idx += sizeof(salt);
				vba += idx + salt;
				salt += idx + vba;
			}
		}


		/************
		 ** Module **
		 ************/

		bool _peek_generated_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override
		{
			/* TRUST_ANCHOR */ {
				Command const cmd {
					peek_pending_command(Command::TRUST_ANCHOR) };

				if (cmd.type() != Command::INVALID) {

					Trust_anchor_node const &node { cmd.trust_anchor_node() };
					switch (node.op()) {
					case Trust_anchor_request::INITIALIZE:

						Trust_anchor_request::create(
							buf_ptr, buf_size, COMMAND_POOL, cmd.id(),
							(unsigned long)Trust_anchor_request::INITIALIZE,
							nullptr, 0, nullptr, nullptr, node.passphrase().string(),
							nullptr);

						return true;

					default: break;
					}
					class Exception_1 { };
					throw Exception_1 { };
				}
			}

			/* INITIALIZE */ {
				Command const cmd {
					peek_pending_command(Command::INITIALIZE) };

				if (cmd.type() != Command::INVALID) {

					Cbe_init::Configuration const &cfg { cmd.initialize() };

					Sb_initializer_request::create(
						buf_ptr, buf_size, COMMAND_POOL, cmd.id(),
						(unsigned long)Sb_initializer_request::INIT,
						nullptr, 0,
						cfg.vbd_nr_of_lvls() - 1,
						cfg.vbd_nr_of_children(),
						cfg.vbd_nr_of_leafs(),
						cfg.ft_nr_of_lvls() - 1,
						cfg.ft_nr_of_children(),
						cfg.ft_nr_of_leafs(),
						cfg.ft_nr_of_lvls() - 1,
						cfg.ft_nr_of_children(),
						cfg.ft_nr_of_leafs());

					return true;
				}
			}

			return false;
		}

		void _drop_generated_request(Module_request &mod_req) override
		{
			switch (mod_req.dst_module_id()) {
			case TRUST_ANCHOR:
			{
				Trust_anchor_request const &ta_req {
					*dynamic_cast<Trust_anchor_request *>(&mod_req)};

				if (ta_req.type() != Trust_anchor_request::INITIALIZE) {
					class Exception_2 { };
					throw Exception_2 { };
				}
				mark_command_in_progress(ta_req.src_request_id());
				break;
			}
			case SB_INITIALIZER:
			{
				Sb_initializer_request const &sb_req {
					*dynamic_cast<Sb_initializer_request *>(&mod_req)};

				if (sb_req.type() != Sb_initializer_request::INIT) {
					class Exception_2 { };
					throw Exception_2 { };
				}
				mark_command_in_progress(sb_req.src_request_id());
				break;
			}
			default:
			{
				class Exception_1 { };
				throw Exception_1 { };
			}
			}
		}

		void generated_request_complete(Module_request &mod_req) override
		{
			switch (mod_req.dst_module_id()) {
			case TRUST_ANCHOR:
			{
				Trust_anchor_request const &ta_req {
					*dynamic_cast<Trust_anchor_request *>(&mod_req)};

				if (ta_req.type() != Trust_anchor_request::INITIALIZE) {
					class Exception_2 { };
					throw Exception_2 { };
				}
				mark_command_completed(
					ta_req.src_request_id(), ta_req.success());
				break;
			}
			case SB_INITIALIZER:
			{
				Sb_initializer_request const &sb_req {
					*dynamic_cast<Sb_initializer_request *>(&mod_req)};

				if (sb_req.type() != Sb_initializer_request::INIT) {
					class Exception_2 { };
					throw Exception_2 { };
				}
				mark_command_completed(
					sb_req.src_request_id(), sb_req.success());
				break;
			}
			case REQUEST_POOL:
			{
				Request const &rp_req {
					*dynamic_cast<Request *>(&mod_req)};

				mark_command_completed(
					rp_req.src_request_id(), rp_req.success());
				break;
			}
			default:
			{
				class Exception_1 { };
				throw Exception_1 { };
			}
			}
		}

	public:

		Command_pool(Allocator          &alloc,
		             Xml_node     const &config_xml,
		             Verbose_node const &verbose_node)
		:
			_alloc        { alloc },
			_verbose_node { verbose_node }
		{
			config_xml.sub_node("commands").for_each_sub_node(
				[&] (Xml_node const &node)
			{
				_read_cmd_node(node, Command::type_from_string(node.type()));
			});
		}

		Command peek_pending_command(Command::Type type) const
		{
			Reconstructible<Command> resulting_cmd { };
			bool first_uncompleted_cmd { true };
			bool exit_loop { false };
			_cmd_queue.for_each([&] (Command &curr_cmd)
			{
				if (exit_loop) {
					return;
				}
				switch (curr_cmd.state()) {
				case Command::PENDING:

					/*
					 * Stop iterating at the first uncompleted command
					 * that needs to be synchronized.
					 */
					if (curr_cmd.synchronize()) {
						if (curr_cmd.type() == type && first_uncompleted_cmd) {
							resulting_cmd.construct(curr_cmd);
						}
						exit_loop = true;
						return;
					}
					/*
					 * Select command and stop iterating if the command is of
					 * the desired type.
					 */
					if (curr_cmd.type() == type) {
						resulting_cmd.construct(curr_cmd);
						exit_loop = true;
					}
					first_uncompleted_cmd = false;
					return;

				case Command::IN_PROGRESS:

					/*
					 * Stop iterating at the first uncompleted command
					 * that needs to be synchronized.
					 */
					if (curr_cmd.synchronize()) {
						exit_loop = true;
						return;
					}
					first_uncompleted_cmd = false;
					return;

				case Command::COMPLETED:

					return;
				}
			});
			return *resulting_cmd;
		}

		void mark_command_in_progress(unsigned long cmd_id)
		{
			bool exit_loop { false };
			_cmd_queue.for_each([&] (Command &cmd)
			{
				if (exit_loop) {
					return;
				}
				if (cmd.id() == cmd_id) {
					if (cmd.state() != Command::PENDING) {
						class Bad_state { };
						throw Bad_state { };
					}
					cmd.state(Command::IN_PROGRESS);
					exit_loop = true;

					if (_verbose_node.cmd_pool_cmd_in_progress()) {
						log("cmd in progress: ", cmd);
					}
				}
			});
		}

		void mark_command_completed(unsigned long cmd_id,
		                            bool          success)
		{
			bool exit_loop { false };
			_cmd_queue.for_each([&] (Command &cmd)
			{
				if (exit_loop) {
					return;
				}
				if (cmd.id() == cmd_id) {

					if (cmd.state() != Command::IN_PROGRESS) {

						class Bad_state { };
						throw Bad_state { };
					}
					cmd.state(Command::COMPLETED);
					_nr_of_uncompleted_cmds--;
					cmd.success(success);
					if (!cmd.success()) {
						_nr_of_errors++;
					}
					exit_loop = true;

					if (_verbose_node.cmd_pool_cmd_completed()) {
						log("cmd completed: ", cmd);
					}
				}
			});
		}

		void generate_blk_data(uint64_t               cbe_req_tag,
		                       Virtual_block_address  vba,
		                       Block_data            &blk_data) const
		{
			bool exit_loop { false };
			_cmd_queue.for_each([&] (Command &cmd)
			{
				if (exit_loop) {
					return;
				}
				if (cmd.id() != cbe_req_tag) {
					return;
				}
				if (cmd.type() != Command::REQUEST) {
					class Bad_command_type { };
					throw Bad_command_type { };
				}
				Request_node const &req_node { cmd.request_node() };
				if (req_node.salt_avail()) {

					_generate_blk_data(blk_data, vba, req_node.salt());
				}
				exit_loop = true;
			});
		}

		void verify_blk_data(uint64_t               cbe_req_tag,
		                     Virtual_block_address  vba,
		                     Block_data            &blk_data)
		{
			bool exit_loop { false };
			_cmd_queue.for_each([&] (Command &cmd)
			{
				if (exit_loop) {
					return;
				}
				if (cmd.id() != cbe_req_tag) {
					return;
				}
				if (cmd.type() != Command::REQUEST) {
					class Bad_command_type { };
					throw Bad_command_type { };
				}
				Request_node const &req_node { cmd.request_node() };
				if (req_node.salt_avail()) {
					Block_data gen_blk_data { };
					_generate_blk_data(gen_blk_data, vba, req_node.salt());

					if (memcmp(blk_data.values, gen_blk_data.values,
					           sizeof(blk_data.values) /
					           sizeof(blk_data.values[0]))) {

						cmd.data_mismatch(true);
						_nr_of_errors++;

						if (_verbose_node.client_data_mismatch()) {
							log("client data mismatch: vba=", vba,
							    " req_tag=(", cbe_req_tag, ")");
							log("client data should be:");
							print_blk_data(gen_blk_data);
							log("client data is:");
							print_blk_data(blk_data);
							class Client_data_mismatch { };
							throw Client_data_mismatch { };
						}
					}
				}
				exit_loop = true;
			});
		}

		void print_failed_cmds() const
		{
			_cmd_queue.for_each([&] (Command &cmd)
			{
				if (cmd.state() != Command::COMPLETED) {
					return;
				}
				if (cmd.success() &&
				    (!cmd.has_attr_data_mismatch() || !cmd.data_mismatch())) {

					return;
				}
				log("cmd failed: ", cmd);
			});
		}

		unsigned long nr_of_uncompleted_cmds() { return _nr_of_uncompleted_cmds; }
		unsigned long nr_of_errors()           { return _nr_of_errors; }
};


static Block_allocator *_block_allocator_ptr;


Genode::uint64_t block_allocator_first_block()
{
	if (!_block_allocator_ptr) {
		struct Exception_1 { };
		throw Exception_1();
	}

	return _block_allocator_ptr->first_block();
}


Genode::uint64_t block_allocator_nr_of_blks()
{
	if (!_block_allocator_ptr) {
		struct Exception_1 { };
		throw Exception_1();
	}

	return _block_allocator_ptr->nr_of_blks();
}


class Main : Vfs::Env::User, public Cbe::Module
{
	private:

		enum { NR_OF_MODULES = 17 };

		Genode::Env                        &_env;
		Attached_rom_dataspace              _config_rom                 { _env, "config" };
		Verbose_node                        _verbose_node               { _config_rom.xml() };
		Heap                                _heap                       { _env.ram(), _env.rm() };
		Vfs::Simple_env                     _vfs_env                    { _env, _heap, _config_rom.xml().sub_node("vfs"), *this };
		Signal_handler<Main>                _sigh                       { _env.ep(), *this, &Main::_execute };
		Command_pool                        _cmd_pool                   { _heap, _config_rom.xml(), _verbose_node };
		Constructible<Free_tree>            _free_tree                  { };
		Constructible<Virtual_block_device> _vbd                        { };
		Constructible<Superblock_control>   _sb_control                 { };
		Constructible<Request_pool>         _request_pool               { };
		Benchmark                           _benchmark                  { _env };
		Meta_tree                           _meta_tree                  { };
		Trust_anchor                        _trust_anchor               { _vfs_env, _config_rom.xml().sub_node("trust-anchor") };
		Crypto                              _crypto                     { _vfs_env, _config_rom.xml().sub_node("crypto") };
		Block_io                            _block_io                   { _vfs_env, _config_rom.xml().sub_node("block-io") };
		Block_allocator                     _block_allocator            { NR_OF_SUPERBLOCK_SLOTS };
		Vbd_initializer                     _vbd_initializer            { };
		Ft_initializer                      _ft_initializer             { };
		Sb_initializer                      _sb_initializer             { };
		Client_data_request                 _client_data_request        { };

		Module *_module_ptrs[NR_OF_MODULES] { };

		/*
		 * Noncopyable
		 */
		Main(Main const &) = delete;
		Main &operator = (Main const &) = delete;

		void _construct_cbe()
		{
			_free_tree.construct();
			_modules_add(FREE_TREE, *_free_tree);

			_vbd.construct();
			_modules_add(VIRTUAL_BLOCK_DEVICE, *_vbd);

			_sb_control.construct();
			_modules_add(SUPERBLOCK_CONTROL, *_sb_control);

			_request_pool.construct();
			_modules_add(REQUEST_POOL, *_request_pool);
		}

		void _destruct_cbe()
		{
			_modules_remove(REQUEST_POOL);
			_request_pool.destruct();

			_modules_remove(SUPERBLOCK_CONTROL);
			_sb_control.destruct();

			_modules_remove(VIRTUAL_BLOCK_DEVICE);
			_vbd.destruct();

			_modules_remove(FREE_TREE);
			_free_tree.destruct();
		}

		/**
		 * Vfs::Env::User interface
		 */
		void wakeup_vfs_user() override { _sigh.local_submit(); }

		template <typename MODULE>
		void _handle_completed_client_requests_of_module(MODULE &module,
		                                                 bool   &progress)
		{
			while (true) {

				Cbe::Request const cbe_req {
					module.peek_completed_client_request() };

				if (!cbe_req.valid()) {
					break;
				}
				_cmd_pool.mark_command_completed(cbe_req.tag(),
				                                 cbe_req.success());

				module.drop_completed_client_request(cbe_req);
				progress = true;
			}
		}

		bool ready_to_submit_request() override
		{
			return _client_data_request._type == Client_data_request::INVALID;
		}

		void submit_request(Module_request &req) override
		{
			if (_client_data_request._type != Client_data_request::INVALID) {

				class Exception_1 { };
				throw Exception_1 { };
			}
			req.dst_request_id(0);
			_client_data_request = *dynamic_cast<Client_data_request *>(&req);
			switch (_client_data_request._type) {
			case Client_data_request::OBTAIN_PLAINTEXT_BLK:

				_cmd_pool.generate_blk_data(
					_client_data_request._client_req_tag,
					_client_data_request._vba,
					*(Block_data *)_client_data_request._plaintext_blk_ptr);

				_benchmark.raise_nr_of_virt_blks_written();

				if (_verbose_node.client_data_transferred())
					log("client data: vba=", _client_data_request._vba,
					    " req_tag=", _client_data_request._client_req_tag);

				_client_data_request._success = true;
				break;

			case Client_data_request::SUPPLY_PLAINTEXT_BLK:

				_cmd_pool.verify_blk_data(
					_client_data_request._client_req_tag,
					_client_data_request._vba,
					*(Block_data *)_client_data_request._plaintext_blk_ptr);

				_benchmark.raise_nr_of_virt_blks_read();

				if (_verbose_node.client_data_transferred())
					log("client data: vba=", _client_data_request._vba,
					    " req_tag=", _client_data_request._client_req_tag);

				_client_data_request._success = true;
				break;

			case Client_data_request::INVALID:

				class Exception_2 { };
				throw Exception_2 { };
			}
		}

		void execute(bool &) override { }

		bool _peek_completed_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override
		{
			if (_client_data_request._type != Client_data_request::INVALID) {
				if (sizeof(_client_data_request) > buf_size) {
					class Exception_1 { };
					throw Exception_1 { };
				}
				Genode::memcpy(buf_ptr, &_client_data_request,
				               sizeof(_client_data_request));;
				return true;
			}
			return false;
		}

		void _drop_completed_request(Module_request &) override
		{
			if (_client_data_request._type == Client_data_request::INVALID) {
				class Exception_2 { };
				throw Exception_2 { };
			}
			_client_data_request._type = Client_data_request::INVALID;
		}

		void _cmd_pool_handle_pending_check_cmds(bool &progress)
		{
			Command const cmd {
				_cmd_pool.peek_pending_command(Command::CHECK) };

			if (cmd.type() == Command::INVALID) {
				return;
			}
			warning("skip <check/> command because it is temporarily not supported");
			_cmd_pool.mark_command_in_progress(cmd.id());
			_cmd_pool.mark_command_completed(cmd.id(), true);
			progress = true;
		}

		void _cmd_pool_handle_pending_cbe_cmds(bool &progress)
		{
			while (true) {

				if (!_request_pool->ready_to_submit_request()) {
					break;
				}
				Command const cmd {
					_cmd_pool.peek_pending_command(Command::REQUEST) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				if (cmd.request_node().op() == Cbe::Request::Operation::REKEY) {
					warning("skip <request op=\"rekey\"/> command because it is temporarily not supported");
					_cmd_pool.mark_command_in_progress(cmd.id());
					_cmd_pool.mark_command_completed(cmd.id(), true);
					progress = true;
					continue;
				}
				if (cmd.request_node().op() == Cbe::Request::Operation::EXTEND_FT) {
					warning("skip <request op=\"extend_ft\"/> command because it is temporarily not supported");
					_cmd_pool.mark_command_in_progress(cmd.id());
					_cmd_pool.mark_command_completed(cmd.id(), true);
					progress = true;
					continue;
				}
				if (cmd.request_node().op() == Cbe::Request::Operation::EXTEND_VBD) {
					warning("skip <request op=\"extend_vbd\"/> command because it is temporarily not supported");
					_cmd_pool.mark_command_in_progress(cmd.id());
					_cmd_pool.mark_command_completed(cmd.id(), true);
					progress = true;
					continue;
				}
				if (cmd.request_node().op() == Cbe::Request::Operation::CREATE_SNAPSHOT) {
					warning("skip <request op=\"create_snapshot\"/> command because it is temporarily not supported");
					_cmd_pool.mark_command_in_progress(cmd.id());
					_cmd_pool.mark_command_completed(cmd.id(), true);
					progress = true;
					continue;
				}
				if (cmd.request_node().op() == Cbe::Request::Operation::DISCARD_SNAPSHOT) {
					warning("skip <request op=\"discard_snapshot\"/> command because it is temporarily not supported");
					_cmd_pool.mark_command_in_progress(cmd.id());
					_cmd_pool.mark_command_completed(cmd.id(), true);
					progress = true;
					continue;
				}
				Request_node req_node { cmd.request_node() };
				Cbe::Request cbe_req {
					cmd.request_node().op(),
					false,
					req_node.has_attr_vba() ? req_node.vba() : 0,
					0,
					req_node.has_attr_count() ? req_node.count() : 0,
					0,
					cmd.id(),
					COMMAND_POOL, cmd.id() };

				_request_pool->submit_request(cbe_req);
				if (VERBOSE_MODULE_COMMUNICATION)
					Genode::log(
						module_name(cbe_req.src_module_id()), ":",
						cbe_req.src_request_id_str(),
						" --", cbe_req.type_name(), "--> ",
						module_name(cbe_req.dst_module_id()), ":",
						cbe_req.dst_request_id_str());

				_cmd_pool.mark_command_in_progress(cmd.id());
				progress = true;
			}
		}

		void _cmd_pool_handle_pending_dump_cmds(bool &progress)
		{
			Command const cmd {
				_cmd_pool.peek_pending_command(Command::DUMP) };

			if (cmd.type() == Command::INVALID) {
				return;
			}
			warning("skip <dump/> command because it is temporarily not supported");
			_cmd_pool.mark_command_in_progress(cmd.id());
			_cmd_pool.mark_command_completed(cmd.id(), true);
			progress = true;
		}

		void _cmd_pool_handle_pending_construct_cmds(bool &progress)
		{
			while (true) {

				Command const cmd {
					_cmd_pool.peek_pending_command(Command::CONSTRUCT) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				_construct_cbe();
				_cmd_pool.mark_command_in_progress(cmd.id());
				_cmd_pool.mark_command_completed(cmd.id(), true);
				progress = true;
			}
		}

		void _cmd_pool_handle_pending_destruct_cmds(bool &progress)
		{
			while (true) {

				Command const cmd {
					_cmd_pool.peek_pending_command(Command::DESTRUCT) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				_destruct_cbe();
				_cmd_pool.mark_command_in_progress(cmd.id());
				_cmd_pool.mark_command_completed(cmd.id(), true);
				progress = true;
			}
		}

		void _cmd_pool_handle_pending_list_snapshots_cmds(bool &progress)
		{
			while (true) {

				Command const cmd {
					_cmd_pool.peek_pending_command(Command::LIST_SNAPSHOTS) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				Active_snapshot_ids ids;
				_sb_control->active_snapshot_ids(ids);
				unsigned snap_nr { 0 };
				log("");
				log("List snapshots (command ID ", cmd.id(), ")");
				for (unsigned idx { 0 }; idx < sizeof(ids.values) / sizeof(ids.values[0]); idx++) {
					if (ids.values[idx] != 0) {
						log("   Snapshot #", snap_nr, " is generation ",
						    (uint64_t)ids.values[idx]);

						snap_nr++;
					}
				}
				log("");
				_cmd_pool.mark_command_in_progress(cmd.id());
				_cmd_pool.mark_command_completed(cmd.id(), true);
				progress = true;
			}
		}

		void _cmd_pool_handle_pending_log_cmds(bool &progress)
		{
			while (true) {

				Command const cmd {
					_cmd_pool.peek_pending_command(Command::LOG) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				log("\n", cmd.log_node().string(), "\n");
				_cmd_pool.mark_command_in_progress(cmd.id());
				_cmd_pool.mark_command_completed(cmd.id(), true);
				progress = true;
			}
		}

		void _cmd_pool_handle_pending_benchmark_cmds(bool &progress)
		{
			while (true) {

				Command const cmd {
					_cmd_pool.peek_pending_command(Command::BENCHMARK) };

				if (cmd.type() == Command::INVALID) {
					break;
				}
				_benchmark.submit_request(cmd.benchmark_node());
				_cmd_pool.mark_command_in_progress(cmd.id());
				_cmd_pool.mark_command_completed(cmd.id(), true);
				progress = true;
			}
		}

		void _execute_command_pool(bool &progress)
		{
			if (_request_pool.constructed()) {
				_cmd_pool_handle_pending_cbe_cmds(progress);
				_cmd_pool_handle_pending_list_snapshots_cmds(progress);
			}
			_cmd_pool_handle_pending_log_cmds(progress);
			_cmd_pool_handle_pending_benchmark_cmds(progress);
			_cmd_pool_handle_pending_construct_cmds(progress);
			_cmd_pool_handle_pending_destruct_cmds(progress);
			_cmd_pool_handle_pending_dump_cmds(progress);
			_cmd_pool_handle_pending_check_cmds(progress);

			if (_cmd_pool.nr_of_uncompleted_cmds() == 0) {

				if (_cmd_pool.nr_of_errors() > 0) {

					_cmd_pool.print_failed_cmds();
					_env.parent().exit(-1);

				} else {

					_env.parent().exit(0);
				}
			}
		}

		void _modules_add(unsigned long  module_id,
		                  Module        &module)
		{
			if (module_id >= NR_OF_MODULES) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			if (_module_ptrs[module_id] != nullptr) {
				class Exception_2 { };
				throw Exception_2 { };
			}
			_module_ptrs[module_id] = &module;
		}

		void _modules_remove(unsigned long  module_id)
		{
			if (module_id >= NR_OF_MODULES) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			if (_module_ptrs[module_id] == nullptr) {
				class Exception_2 { };
				throw Exception_2 { };
			}
			_module_ptrs[module_id] = nullptr;
		}

		void _modules_execute(bool &progress)
		{
			for (unsigned long id { 0 }; id < NR_OF_MODULES; id++) {

				if (_module_ptrs[id] == nullptr)
					continue;

				Module *module_ptr { _module_ptrs[id] };
				module_ptr->execute(progress);
				module_ptr->for_each_generated_request([&] (Module_request &req) {
					if (req.dst_module_id() >= NR_OF_MODULES) {
						class Exception_1 { };
						throw Exception_1 { };
					}
					Module &dst_module { *_module_ptrs[req.dst_module_id()] };
					if (!dst_module.ready_to_submit_request()) {

						if (VERBOSE_MODULE_COMMUNICATION)
							Genode::log(
								module_name(id), ":", req.src_request_id_str(),
								" --", req.type_name(), "-| ",
								module_name(req.dst_module_id()));

						return Module::REQUEST_NOT_HANDLED;
					}
					dst_module.submit_request(req);

					if (VERBOSE_MODULE_COMMUNICATION)
						Genode::log(
							module_name(id), ":", req.src_request_id_str(),
							" --", req.type_name(), "--> ",
							module_name(req.dst_module_id()), ":",
							req.dst_request_id_str());

					progress = true;
					return Module::REQUEST_HANDLED;
				});
				module_ptr->for_each_completed_request([&] (Module_request &req) {
					if (req.src_module_id() >= NR_OF_MODULES) {
						class Exception_2 { };
						throw Exception_2 { };
					}
					if (VERBOSE_MODULE_COMMUNICATION)
						Genode::log(
							module_name(req.src_module_id()), ":",
							req.src_request_id_str(), " <--", req.type_name(),
							"-- ", module_name(id), ":",
							req.dst_request_id_str());
					Module &src_module { *_module_ptrs[req.src_module_id()] };
					src_module.generated_request_complete(req);
					progress = true;
				});
			}
		}

		void _execute()
		{
			bool progress { true };
			while (progress) {

				progress = false;
				_execute_command_pool(progress);
				_modules_execute(progress);
			}
			_vfs_env.io().commit();
		}

	public:

		Main(Genode::Env &env)
		:
			_env { env }
		{
			_modules_add(META_TREE,         _meta_tree);
			_modules_add(CRYPTO,            _crypto);
			_modules_add(TRUST_ANCHOR,      _trust_anchor);
			_modules_add(CLIENT_DATA,      *this);
			_modules_add(COMMAND_POOL,      _cmd_pool);
			_modules_add(BLOCK_IO,          _block_io);
			_modules_add(BLOCK_ALLOCATOR,   _block_allocator);
			_modules_add(VBD_INITIALIZER,   _vbd_initializer);
			_modules_add(FT_INITIALIZER,    _ft_initializer);
			_modules_add(SB_INITIALIZER,    _sb_initializer);

			_block_allocator_ptr = &_block_allocator;

			_execute();
		}
};

namespace Libc {

	struct Env;

	struct Component
	{
		void construct(Libc::Env &);
	};
}


void Libc::Component::construct(Libc::Env &) { }


void Component::construct(Genode::Env &env)
{
	env.exec_static_constructors();

	static Main main(env);

	(void)block_allocator_first_block();
	(void)block_allocator_nr_of_blks();
}

extern "C" int memcmp(const void *p0, const void *p1, Genode::size_t size)
{
	return Genode::memcmp(p0, p1, size);
}
