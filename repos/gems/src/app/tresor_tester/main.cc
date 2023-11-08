/*
 * \brief  Tool for running tests and benchmarks on Tresor library
 * \author Martin Stein
 * \date   2020-08-26
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <util/avl_tree.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <timer_session/connection.h>
#include <vfs/simple_env.h>

/* tresor init configuration */
#include <tresor_init/configuration.h>

/* tresor includes */
#include <tresor/crypto.h>
#include <tresor/trust_anchor.h>
#include <tresor/client_data.h>
#include <tresor/block_io.h>
#include <tresor/meta_tree.h>
#include <tresor/free_tree.h>
#include <tresor/request_pool.h>
#include <tresor/vbd_initializer.h>
#include <tresor/ft_initializer.h>
#include <tresor/sb_initializer.h>
#include <tresor/sb_check.h>
#include <tresor/vbd_check.h>
#include <tresor/ft_check.h>
#include <tresor/virtual_block_device.h>
#include <tresor/superblock_control.h>

using namespace Genode;
using namespace Tresor;
using namespace Vfs;

namespace Tresor_tester {

	class Client_data;
	class Main;
}


template <typename T>
T read_attribute(Xml_node const &node,
                 char const *attr)
{
	T value { };
	ASSERT(node.has_attribute(attr));
	ASSERT(node.attribute(attr).value(value));
	return value;
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
		bool const _label_avail;
		Label const _label;

		Operation _read_op_attr(Xml_node const &node)
		{
			ASSERT(node.has_attribute("op"));
			if (node.attribute("op").has_value("start")) return Operation::START;
			if (node.attribute("op").has_value("stop")) return Operation::STOP;
			ASSERT_NEVER_REACHED;
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
			_op { _read_op_attr(node) },
			_label_avail { has_attr_label() && node.has_attribute("label") },
			_label { _label_avail ?
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

		Genode::Env &_env;
		Timer::Connection _timer { _env };
		State _state { STOPPED };
		Microseconds _start_time { 0 };
		uint64_t _nr_of_virt_blks_read { 0 };
		uint64_t _nr_of_virt_blks_written { 0 };
		Constructible<Benchmark_node> _start_node { };
		uint64_t _id { 0 };

	public:

		Benchmark(Genode::Env &env) : _env { env } { }

		void execute_cmd(Benchmark_node const &node)
		{
			switch (node.op()) {
			case Benchmark_node::START:

				ASSERT(_state == STOPPED);
				_id++;
				_nr_of_virt_blks_read = 0;
				_nr_of_virt_blks_written = 0;
				_state = STARTED;
				_start_node.construct(node);
				_start_time = _timer.curr_time().trunc_to_plain_us();
				break;

			case Benchmark_node::STOP:

				ASSERT(_state == STARTED);
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
						_nr_of_virt_blks_read * Tresor::BLOCK_SIZE };

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
						_nr_of_virt_blks_written * Tresor::BLOCK_SIZE };

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

		void raise_nr_of_virt_blks_read() { _nr_of_virt_blks_read++; }
		void raise_nr_of_virt_blks_written() { _nr_of_virt_blks_written++; }
};


class Trust_anchor_node
{
	private:

		using Operation = Trust_anchor_request::Type;

		Operation const _op;
		Passphrase const _passphrase;

		Operation _read_op_attr(Xml_node const &node)
		{
			ASSERT(node.has_attribute("op"));
			if (node.attribute("op").has_value("initialize")) return Operation::INITIALIZE;
			ASSERT_NEVER_REACHED;
		}

	public:

		Trust_anchor_node(Xml_node const &node)
		:
			_op { _read_op_attr(node) },
			_passphrase { has_attr_passphrase() ?
			              node.attribute_value("passphrase", Passphrase()) :
			              Passphrase() }
		{ }

		Operation op() const { return _op; }
		Passphrase const &passphrase() const { return _passphrase; }

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

		using Operation = Tresor::Request::Operation;

		Operation const _op;
		Virtual_block_address const _vba;
		Number_of_blocks const _count;
		bool const _sync;
		bool const _salt_avail;
		uint64_t const _salt;
		Snapshot_id const _snap_id;

		Operation _read_op_attr(Xml_node const &node)
		{
			ASSERT(node.has_attribute("op"));
			if (node.attribute("op").has_value("read")) return Operation::READ;
			if (node.attribute("op").has_value("write")) return Operation::WRITE;
			if (node.attribute("op").has_value("sync")) return Operation::SYNC;
			if (node.attribute("op").has_value("create_snapshot")) return Operation::CREATE_SNAPSHOT;
			if (node.attribute("op").has_value("discard_snapshot")) return Operation::DISCARD_SNAPSHOT;
			if (node.attribute("op").has_value("extend_ft")) return Operation::EXTEND_FT;
			if (node.attribute("op").has_value("extend_vbd")) return Operation::EXTEND_VBD;
			if (node.attribute("op").has_value("rekey")) return Operation::REKEY;
			if (node.attribute("op").has_value("deinitialize")) return Operation::DEINITIALIZE;
			ASSERT_NEVER_REACHED;
		}

	public:

		Request_node(Xml_node const &node)
		:
			_op { _read_op_attr(node) },
			_vba { has_attr_vba() ? read_attribute<uint64_t>(node, "vba") : 0 },
			_count { has_attr_count() ? read_attribute<uint64_t>(node, "count") : 0 },
			_sync { read_attribute<bool>(node, "sync") },
			_salt_avail { has_attr_salt() ? node.has_attribute("salt") : false },
			_salt { has_attr_salt() && _salt_avail ? read_attribute<uint64_t>(node, "salt") : 0 },
			_snap_id { has_attr_snap_id() ? read_attribute<Snapshot_id>(node, "id") : 0 }
		{ }

		Operation op() const { return _op; }
		Virtual_block_address vba() const { return _vba; }
		Number_of_blocks count() const { return _count; }
		bool sync() const { return _sync; }
		bool salt_avail() const { return _salt_avail; }
		uint64_t salt() const { return _salt; }
		Snapshot_id snap_id() const { return _snap_id; }

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

		bool has_attr_snap_id() const
		{
			return _op == Operation::DISCARD_SNAPSHOT ||
			       _op == Operation::CREATE_SNAPSHOT;
		}

		void print(Genode::Output &out) const
		{
			Genode::print(out, "op=", Request::op_to_string(_op));
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


class Command : public Module_channel
{
	public:

		enum Type {
			INVALID, REQUEST, TRUST_ANCHOR, BENCHMARK, CONSTRUCT, DESTRUCT, INITIALIZE,
			CHECK, CHECK_SNAPSHOTS, LOG };

		enum State { PENDING, IN_PROGRESS, CREATE_SNAP_COMPLETED, DISCARD_SNAP_COMPLETED, COMPLETED };

	private:

		Tresor_tester::Main &_main;
		Type _type { INVALID };
		uint32_t _id { 0 };
		State _state { PENDING };
		bool _success { false };
		Generation _gen { INVALID_GENERATION };
		bool _data_mismatch { false };
		Constructible<Request_node> _request_node { };
		Constructible<Trust_anchor_node> _trust_anchor_node { };
		Constructible<Benchmark_node> _benchmark_node { };
		Constructible<Log_node> _log_node { };
		Constructible<Tresor_init::Configuration> _initialize { };

		void _generated_req_completed(State_uint state_uint) override;

		void _request_submitted(Module_request &) override { ASSERT_NEVER_REACHED; }

		bool _request_complete() override { return false; }

		char const *_state_to_string() const
		{
			switch (_state) {
			case PENDING: return "pending";
			case IN_PROGRESS: return "in_progress";
			case COMPLETED: return "completed";
			default: break;
			}
			return "?";
		}

		char const *_type_to_string() const
		{
			switch (_type) {
			case INITIALIZE: return "initialize";
			case INVALID: return "invalid";
			case REQUEST: return "request";
			case TRUST_ANCHOR: return "trust_anchor";
			case BENCHMARK: return "benchmark";
			case CONSTRUCT: return "construct";
			case DESTRUCT: return "destruct";
			case CHECK: return "check";
			case CHECK_SNAPSHOTS: return "check_snapshots";
			case LOG: return "log";
			}
			return "?";
		}

		static Type _type_from_string(String<64> str)
		{
			if (str == "initialize") { return INITIALIZE; }
			if (str == "request") { return REQUEST; }
			if (str == "trust-anchor") { return TRUST_ANCHOR; }
			if (str == "benchmark") { return BENCHMARK; }
			if (str == "construct") { return CONSTRUCT; }
			if (str == "destruct") { return DESTRUCT; }
			if (str == "check") { return CHECK; }
			if (str == "check-snapshots") { return CHECK_SNAPSHOTS; }
			if (str == "log") { return LOG; }
			ASSERT_NEVER_REACHED;
		}

	public:

		Command(Xml_node const &node, Tresor_tester::Main &main, uint32_t id)
		:
			Module_channel { COMMAND_POOL, id }, _main { main }, _type { _type_from_string(node.type()) }, _id { id }
		{
			switch (_type) {
			case INITIALIZE: _initialize.construct(node); break;
			case REQUEST: _request_node.construct(node); break;
			case TRUST_ANCHOR: _trust_anchor_node.construct(node); break;
			case BENCHMARK: _benchmark_node.construct(node); break;
			case LOG: _log_node.construct(node); break;
			default: break;
			}
		}

		bool has_attr_data_mismatch() const
		{
			return _type == REQUEST && _request_node->op() == Tresor::Request::Operation::READ &&
			       _request_node->salt_avail();
		}

		bool synchronize() const
		{
			switch (_type) {
			case INITIALIZE: return true;
			case BENCHMARK: return true;
			case CONSTRUCT: return true;
			case DESTRUCT: return true;
			case CHECK: return true;
			case TRUST_ANCHOR: return true;
			case CHECK_SNAPSHOTS: return true;
			case LOG: return true;
			case REQUEST: return _request_node->sync();
			case INVALID: break;
			}
			ASSERT_NEVER_REACHED;
		}

		void print(Genode::Output &out) const
		{
			Genode::print(out, "id=", _id, " type=", _type_to_string());
			switch (_type) {
			case INITIALIZE: Genode::print(out, " cfg=(", *_initialize, ")"); break;
			case REQUEST: Genode::print(out, " cfg=(", *_request_node, ")"); break;
			case TRUST_ANCHOR: Genode::print(out, " cfg=(", *_trust_anchor_node, ")"); break;
			case BENCHMARK: Genode::print(out, " cfg=(", *_benchmark_node, ")"); break;
			case LOG: Genode::print(out, " cfg=(", *_log_node, ")"); break;
			case INVALID: break;
			case CHECK: break;
			case CONSTRUCT: break;
			case DESTRUCT: break;
			case CHECK_SNAPSHOTS: break;
			}
			Genode::print(out, " succ=", _success);
			if (has_attr_data_mismatch())
				Genode::print(out, " bad_data=", _data_mismatch);

			Genode::print(out, " state=", _state_to_string());
		}

		Type type () const { return _type ; }
		State state () const { return _state ; }
		uint32_t id () const { return _id ; }
		bool success () const { return _success ; }
		bool data_mismatch () const { return _data_mismatch ; }
		Request_node const &request_node () const { return *_request_node ; }
		Trust_anchor_node const &trust_anchor_node () const { return *_trust_anchor_node; }
		Benchmark_node const &benchmark_node () const { return *_benchmark_node ; }
		Log_node const &log_node () const { return *_log_node ; }
		Tresor_init::Configuration const &initialize () const { return *_initialize ; }

		void state (State state) { _state = state; }
		void success (bool success) { _success = success; }
		void data_mismatch (bool data_mismatch) { _data_mismatch = data_mismatch; }

		void execute(bool &progress);
};


class Snapshot_reference : public Genode::Avl_node<Snapshot_reference>
{
	private:

		Snapshot_id const _id;
		Generation const _gen;

	public:

		Snapshot_reference(Snapshot_id id, Generation gen) : _id { id }, _gen { gen } { }

		template <typename FUNC>
		void with_ref(Snapshot_id id, FUNC && func) const
		{
			if (id != _id) {
				Snapshot_reference *child_ptr { Avl_node<Snapshot_reference>::child(id > _id) };
				if (child_ptr)
					child_ptr->with_ref(id, func);
				else
					ASSERT_NEVER_REACHED;
			} else
				func(*this);
		}

		void print(Genode::Output &out) const { Genode::print(out, "id ", _id, " gen ", _gen); }

		bool higher(Snapshot_reference *other_ptr) { return other_ptr->_id > _id; }

		Snapshot_id id() const { return _id; }
		Generation gen() const { return _gen; }
};


struct Snapshot_reference_tree : public Avl_tree<Snapshot_reference>
{
	template <typename FUNC>
	void with_ref(Snapshot_id id, FUNC && func) const
	{
		if (first())
			first()->with_ref(id, func);
		else
			ASSERT_NEVER_REACHED;
	}
};


class Tresor_tester::Client_data : public Tresor::Module
{
	private:

		Main &_main;
		Constructible<Client_data_request> _request { };


		/********************
		 ** Tresor::Module **
		 ********************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		bool _peek_completed_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t buf_size) override;

		void _drop_completed_request(Module_request &) override;

		bool new_submit_request() override { return false; }

	public:

		Client_data(Main &main) : _main { main } { }
};


class Tresor_tester::Main
:
	private Vfs::Env::User,
	private Tresor::Module_composition,
	public Tresor::Module
{
	private:

		Genode::Env &_env;
		Attached_rom_dataspace _config_rom { _env, "config" };
		Heap _heap { _env.ram(), _env.rm() };
		Vfs::Simple_env _vfs_env { _env, _heap, _config_rom.xml().sub_node("vfs"), *this };
		Signal_handler<Main> _signal_handler { _env.ep(), *this, &Main::_handle_signal };
		Benchmark _benchmark { _env };
		uint32_t _next_command_id { 0 };
		unsigned long _nr_of_uncompleted_cmds { 0 };
		unsigned long _nr_of_errors { 0 };
		Tresor::Block _blk_data { };
		Snapshot_reference_tree _snap_refs { };
		Constructible<Free_tree> _free_tree { };
		Constructible<Virtual_block_device> _vbd { };
		Constructible<Superblock_control> _sb_control { };
		Constructible<Request_pool> _request_pool { };
		Constructible<Client_data> _client_data { };
		Constructible<Meta_tree> _meta_tree { };
		Trust_anchor _trust_anchor { _vfs_env, _config_rom.xml().sub_node("trust-anchor") };
		Crypto _crypto { _vfs_env, _config_rom.xml().sub_node("crypto") };
		Block_io _block_io { _vfs_env, _config_rom.xml().sub_node("block-io") };
		Pba_allocator _pba_alloc { NR_OF_SUPERBLOCK_SLOTS };
		Vbd_initializer _vbd_initializer { };
		Ft_initializer _ft_initializer { };
		Sb_initializer _sb_initializer { };
		Sb_check _sb_check { };
		Vbd_check _vbd_check { };
		Ft_check _ft_check { };
		bool _generated_req_success { false };

		static void _generate_blk_data(Tresor::Block &blk_data,
		                               Virtual_block_address vba,
		                               uint64_t salt)
		{
			for (uint64_t idx { 0 }; idx + sizeof(vba) + sizeof(salt) <= BLOCK_SIZE; ) {

				memcpy(&blk_data.bytes[idx], &vba, sizeof(vba));
				idx += sizeof(vba);
				memcpy(&blk_data.bytes[idx], &salt, sizeof(salt));
				idx += sizeof(salt);
				vba += idx + salt;
				salt += idx + vba;
			}
		}

		template <typename FUNC>
		void _with_first_processable_cmd(FUNC && func)
		{
			bool first_uncompleted_cmd { true };
			bool done { false };
			for_each_channel<Command>([&] (Command &cmd)
			{
				if (done)
					return;

				if (cmd.state() == Command::PENDING) {
					done = true;
					if (first_uncompleted_cmd || !cmd.synchronize())
						func(cmd);
				}
				if (cmd.state() == Command::IN_PROGRESS) {
					if (cmd.synchronize())
						done = true;
					else
						first_uncompleted_cmd = false;
				}
			});
		}

		void _try_end_program()
		{
			if (_nr_of_uncompleted_cmds == 0) {
				if (_nr_of_errors > 0) {
					for_each_channel<Command>([&] (Command &cmd) {
						if (cmd.state() != Command::COMPLETED)
							return;

						if (cmd.success() && (!cmd.has_attr_data_mismatch() || !cmd.data_mismatch()))
							return;

						log("cmd failed: ", cmd);
					});
					_env.parent().exit(-1);
				} else
					_env.parent().exit(0);
			}
		}

		void _wakeup_back_end_services() { _vfs_env.io().commit(); }

		void _handle_signal()
		{
			execute_modules();
			_try_end_program();
			_wakeup_back_end_services();
		}


		/****************************
		 ** Make class noncopyable **
		 ****************************/

		Main(Main const &) = delete;

		Main &operator = (Main const &) = delete;


		/********************
		 ** Vfs::Env::User **
		 ********************/

		void wakeup_vfs_user() override { _signal_handler.local_submit(); }


		/********************
		 ** Tresor::Module **
		 ********************/

		void execute(bool &progress) override
		{
			_with_first_processable_cmd([&] (Command &cmd) {
				cmd.execute(progress); });
		}

		bool new_submit_request() override { return false; }

		void _remove_snap_ref(Snapshot_reference &ref)
		{
			_snap_refs.remove(&ref);
			ref.~Snapshot_reference();
			destroy(_heap, &ref);
		}

	public:

		Main(Genode::Env &env) : _env { env }
		{
			add_module(CRYPTO, _crypto);
			add_module(TRUST_ANCHOR, _trust_anchor);
			add_module(COMMAND_POOL, *this);
			add_module(BLOCK_IO, _block_io);
			add_module(VBD_INITIALIZER, _vbd_initializer);
			add_module(FT_INITIALIZER, _ft_initializer);
			add_module(SB_INITIALIZER, _sb_initializer);
			add_module(SB_CHECK, _sb_check);
			add_module(VBD_CHECK, _vbd_check);
			add_module(FT_CHECK, _ft_check);

			_config_rom.xml().sub_node("commands").for_each_sub_node([&] (Xml_node const &node) {
				add_channel(*new (_heap) Command(node, *this, _next_command_id++));
				_nr_of_uncompleted_cmds++;
			});
			_handle_signal();
		}

		void mark_command_in_progress(Module_request_id cmd_id)
		{
			with_channel<Command>(cmd_id, [&] (Command &cmd) {
				ASSERT(cmd.state() == Command::PENDING);
				cmd.state(Command::IN_PROGRESS);
			});
		}

		void mark_command_completed(Module_request_id cmd_id,
		                            bool success)
		{
			with_channel<Command>(cmd_id, [&] (Command &cmd) {
				ASSERT(cmd.state() == Command::IN_PROGRESS);
				cmd.state(Command::COMPLETED);
				_nr_of_uncompleted_cmds--;
				cmd.success(success);
				if (!cmd.success()) {
					warning("cmd ", cmd, " failed");
					_nr_of_errors++;
				}
			});
		}

		Generation snap_id_to_gen(Snapshot_id id)
		{
			Generation gen { INVALID_GENERATION };
			_snap_refs.with_ref(id, [&] (Snapshot_reference const &ref) {
				gen = ref.gen(); });

			return gen;
		}

		void add_snap_ref(Snapshot_id id, Generation gen)
		{
			_snap_refs.insert(new (_heap) Snapshot_reference { id, gen });
		}

		void remove_snap_refs_with_same_gen(Snapshot_id id)
		{
			Generation gen { snap_id_to_gen(id) };
			while (1) {
				Snapshot_reference *ref_ptr { nullptr };
				_snap_refs.for_each([&] (Snapshot_reference const &ref) {
					if (!ref_ptr && ref.gen() == gen)
						ref_ptr = const_cast<Snapshot_reference *>(&ref);
				});
				if (ref_ptr)
					_remove_snap_ref(*ref_ptr);
				else
					break;
			}
		}

		void reset_snap_refs()
		{
			while (_snap_refs.first())
				_remove_snap_ref(*_snap_refs.first());
		}

		Pba_allocator &pba_alloc() { return _pba_alloc; }

		void generate_blk_data(uint64_t tresor_req_tag,
		                       Virtual_block_address vba,
		                       Tresor::Block &blk_data)
		{
			with_channel<Command>(tresor_req_tag, [&] (Command &cmd) {
				ASSERT(cmd.type() == Command::REQUEST);
				Request_node const &req_node { cmd.request_node() };
				if (req_node.salt_avail())
					_generate_blk_data(blk_data, vba, req_node.salt());
			});
			_benchmark.raise_nr_of_virt_blks_written();
		}

		void verify_blk_data(uint64_t tresor_req_tag,
		                     Virtual_block_address vba,
		                     Tresor::Block &blk_data)
		{
			with_channel<Command>(tresor_req_tag, [&] (Command &cmd) {
				ASSERT(cmd.type() == Command::REQUEST);
				Request_node const &req_node { cmd.request_node() };
				if (req_node.salt_avail()) {
					Tresor::Block gen_blk_data { };
					_generate_blk_data(gen_blk_data, vba, req_node.salt());

					if (memcmp(&blk_data, &gen_blk_data, BLOCK_SIZE)) {
						cmd.data_mismatch(true);
						warning("client data mismatch: vba=", vba, " req_tag=", tresor_req_tag);
						_nr_of_errors++;
					}
				}
			});
			_benchmark.raise_nr_of_virt_blks_read();
		}

		void construct_tresor_modules()
		{
			_free_tree.construct();
			_vbd.construct();
			_sb_control.construct();
			_request_pool.construct();
			_client_data.construct(*this);
			_meta_tree.construct();
			add_module(FREE_TREE, *_free_tree);
			add_module(VIRTUAL_BLOCK_DEVICE, *_vbd);
			add_module(SUPERBLOCK_CONTROL, *_sb_control);
			add_module(REQUEST_POOL, *_request_pool);
			add_module(CLIENT_DATA, *_client_data);
			add_module(META_TREE, *_meta_tree);
		}

		void destruct_tresor_modules()
		{
			remove_module(META_TREE);
			remove_module(CLIENT_DATA);
			remove_module(REQUEST_POOL);
			remove_module(SUPERBLOCK_CONTROL);
			remove_module(VIRTUAL_BLOCK_DEVICE);
			remove_module(FREE_TREE);
			_meta_tree.destruct();
			_client_data.destruct();
			_request_pool.destruct();
			_sb_control.destruct();
			_vbd.destruct();
			_free_tree.destruct();
		}

		void check_snapshots(Command &cmd, bool &progress)
		{
			mark_command_in_progress(cmd.id());
			bool success { true };
			Snapshots_info snap_info { _sb_control->snapshots_info() };
			bool snap_gen_ok[MAX_NR_OF_SNAPSHOTS] { false };
			_snap_refs.for_each([&] (Snapshot_reference const &snap_ref) {
				bool snap_ref_ok { false };
				for (Snapshot_index idx { 0 }; idx < MAX_NR_OF_SNAPSHOTS; idx++) {
					if (snap_info.generations[idx] == snap_ref.gen()) {
						snap_ref_ok = true;
						snap_gen_ok[idx] = true;
					}
				}
				if (!snap_ref_ok) {
					warning("snap (", snap_ref, ") not known to tresor");
					_nr_of_errors++;
					success = false;
				}
			});
			for (Snapshot_index idx { 0 }; idx < MAX_NR_OF_SNAPSHOTS; idx++) {
				if (snap_info.generations[idx] != INVALID_GENERATION && !snap_gen_ok[idx]) {
					warning("snap (idx ", idx, " gen ", snap_info.generations[idx], ") not known to tester");
					_nr_of_errors++;
					success = false;
				}
			}
			mark_command_completed(cmd.id(), success);
			progress = true;
		}

		Benchmark &benchmark() { return _benchmark; }
};


/********************************
 ** Tresor_tester::Client_data **
 ********************************/

bool Tresor_tester::Client_data::ready_to_submit_request()
{
	return !_request.constructed();
}

void Tresor_tester::Client_data::submit_request(Module_request &mod_req)
{
	ASSERT(!_request.constructed());
	Client_data_request &req { *static_cast<Client_data_request *>(&mod_req) };
	req.dst_chan_id(0);
	_request.construct(req.src_module_id(), req.src_chan_id(), req._type, req._req_off, req._req_tag, req._pba, req._vba, req._blk, req._success);
	_request->dst_chan_id(0);
	switch (_request->_type) {
	case Client_data_request::OBTAIN_PLAINTEXT_BLK:
		_main.generate_blk_data(_request->_req_tag, _request->_vba, _request->_blk);
		break;
	case Client_data_request::SUPPLY_PLAINTEXT_BLK:
		_main.verify_blk_data(_request->_req_tag, _request->_vba, _request->_blk);
		break;
	}
	_request->_success = true;
}

bool Tresor_tester::Client_data::_peek_completed_request(Genode::uint8_t *buf_ptr,
                                                         Genode::size_t buf_size)
{
	if (!_request.constructed())
		return false;

	ASSERT(sizeof(Client_data_request) <= buf_size);
	construct_at<Client_data_request>(
		buf_ptr, _request->src_module_id(), _request->src_chan_id(), _request->_type, _request->_req_off, _request->_req_tag,
		_request->_pba, _request->_vba, _request->_blk, _request->_success);;
	return true;
}

void Tresor_tester::Client_data::_drop_completed_request(Module_request &)
{
	ASSERT(_request.constructed());
	_request.destruct();
}


void Command::_generated_req_completed(State_uint state_uint)
{
	if (state_uint == CREATE_SNAP_COMPLETED)
		_main.add_snap_ref(request_node().snap_id(), _gen);

	if (state_uint == DISCARD_SNAP_COMPLETED)
		_main.remove_snap_refs_with_same_gen(request_node().snap_id());

	_main.mark_command_completed(id(), _success);
}


void Command::execute(bool &progress)
{
	switch (type()) {
	case REQUEST:
	{
		Request_node node { request_node() };
		State state { COMPLETED };
		_gen = INVALID_GENERATION;
		if (node.op() == Request::DISCARD_SNAPSHOT) {
			_gen = _main.snap_id_to_gen(node.snap_id());
			state = DISCARD_SNAP_COMPLETED;
		}
		if (node.op() == Request::CREATE_SNAPSHOT)
			state = CREATE_SNAP_COMPLETED;

		generate_req<Tresor::Request>(
			state, progress, node.op(), _success, node.has_attr_vba() ? node.vba() : 0,
			0, node.has_attr_count() ? node.count() : 0, 0, id(), _gen);

		_main.mark_command_in_progress(id());
		break;
	}
	case Command::TRUST_ANCHOR:
	{
		Trust_anchor_node node { trust_anchor_node() };
		ASSERT(node.op() == Trust_anchor_request::INITIALIZE);
		generate_req<Trust_anchor::Initialize>(COMPLETED, progress, node.passphrase(), _success);
		_main.mark_command_in_progress(id());
		break;
	}
	case Command::INITIALIZE:
	{
		_main.reset_snap_refs();
		Tresor_init::Configuration const &cfg { initialize() };
		generate_req<Sb_initializer_request>(COMPLETED, progress,
			(Tree_level_index)(cfg.vbd_nr_of_lvls() - 1),
			(Tree_degree)cfg.vbd_nr_of_children(),
			cfg.vbd_nr_of_leafs(),
			(Tree_level_index)cfg.ft_nr_of_lvls() - 1,
			(Tree_degree)cfg.ft_nr_of_children(),
			cfg.ft_nr_of_leafs(),
			(Tree_level_index)cfg.ft_nr_of_lvls() - 1,
			(Tree_degree)cfg.ft_nr_of_children(),
			cfg.ft_nr_of_leafs(), _main.pba_alloc(), _success);
		_main.mark_command_in_progress(id());
		break;
	}
	case Command::CHECK:
		generate_req<Sb_check_request>(COMPLETED, progress, _success);
		_main.mark_command_in_progress(id());
		break;
	case LOG:
		log("\n", log_node().string(), "\n");
		_main.mark_command_in_progress(id());
		_main.mark_command_completed(id(), true);
		progress = true;
		break;
	case BENCHMARK:
		_main.benchmark().execute_cmd(benchmark_node());
		_main.mark_command_in_progress(id());
		_main.mark_command_completed(id(), true);
		progress = true;
		break;
	case CONSTRUCT:
		_main.construct_tresor_modules();
		_main.mark_command_in_progress(id());
		_main.mark_command_completed(id(), true);
		progress = true;
		break;
	case DESTRUCT:
		_main.destruct_tresor_modules();
		_main.mark_command_in_progress(id());
		_main.mark_command_completed(id(), true);
		progress = true;
		break;
	case CHECK_SNAPSHOTS: _main.check_snapshots(*this, progress); break;
	default: break;
	}
}


/*********************
 ** Libc::Component **
 *********************/

namespace Libc {

	struct Env;

	struct Component
	{
		void construct(Libc::Env &) { }
	};
}


/***********************
 ** Genode::Component **
 ***********************/

void Component::construct(Genode::Env &env)
{
	env.exec_static_constructors();

	static Tresor_tester::Main main(env);
}

extern "C" int memcmp(const void *p0, const void *p1, Genode::size_t size)
{
	return Genode::memcmp(p0, p1, size);
}
