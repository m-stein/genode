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
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <timer_session/connection.h>
#include <vfs/simple_env.h>

/* tresor init includes */
#include <tresor_init/configuration.h>

/* tresor includes */
#include <tresor/crypto.h>
#include <tresor/trust_anchor.h>
#include <tresor/client_data_interface.h>
#include <tresor/block_io.h>
#include <tresor/meta_tree.h>
#include <tresor/free_tree.h>
#include <tresor/request_scheduler.h>
#include <tresor/vbd_initializer.h>
#include <tresor/ft_initializer.h>
#include <tresor/sb_initializer.h>
#include <tresor/sb_check.h>
#include <tresor/vbd_check.h>
#include <tresor/ft_check.h>
#include <tresor/virtual_block_device.h>
#include <tresor/superblock_control.h>

namespace Tresor_tester {

	enum { VERBOSE = 0 };

	using namespace Genode;
	using namespace Tresor;

	using Salt = uint64_t;
	using Command_id = uint64_t;

	class Log_node;
	class Benchmark_node;
	class Benchmark;
	class Command;
	class Initialize_trust_anchor_node;
	class Snapshot_reference;
	class Snapshot_reference_tree;
	class Request_node;
	class Crypto_key;
	class Main;

	template <typename T>
	T read_attribute(Xml_node const &node, char const *attr)
	{
		T value { };
		ASSERT(node.has_attribute(attr));
		ASSERT(node.attribute(attr).value(value));
		return value;
	}
}

struct Tresor_tester::Benchmark_node : Noncopyable
{
	using Label = String<128>;

	enum Operation { START, STOP };

	Operation const op;
	bool const label_avail;
	Label const label;

	Operation read_op_attr(Xml_node const &node)
	{
		ASSERT(node.has_attribute("op"));
		if (node.attribute("op").has_value("start")) return START;
		if (node.attribute("op").has_value("stop")) return STOP;
		ASSERT_NEVER_REACHED;
	}

	Benchmark_node(Xml_node const &node)
	:
		op(read_op_attr(node)), label_avail(op == START && node.has_attribute("label")),
		label (label_avail ? node.attribute_value("label", Label()) : Label())
	{ }
};


class Tresor_tester::Benchmark : Noncopyable
{
	private:

		enum State { STARTED, STOPPED };

		Genode::Env &_env;
		Timer::Connection _timer { _env };
		State _state { STOPPED };
		Microseconds _start_time { 0 };
		Number_of_blocks _num_virt_blks_read { 0 };
		Number_of_blocks _num_virt_blks_written { 0 };
		Benchmark_node const *_start_node_ptr { };

		/*
		 * Noncopyable
		 */
		Benchmark(Benchmark const &) = delete;
		Benchmark &operator = (Benchmark const &) = delete;

	public:

		Benchmark(Genode::Env &env) : _env(env) { }

		void execute_cmd(Benchmark_node const &node)
		{
			switch (node.op) {
			case Benchmark_node::START:

				ASSERT(_state == STOPPED);
				_num_virt_blks_read = 0;
				_num_virt_blks_written = 0;
				_state = STARTED;
				_start_node_ptr = &node;
				_start_time = _timer.curr_time().trunc_to_plain_us();
				break;

			case Benchmark_node::STOP:

				ASSERT(_state == STARTED);
				uint64_t const stop_time_us { _timer.curr_time().trunc_to_plain_us().value };
				log("");
				if (_start_node_ptr->label_avail)
					log("Benchmark result \"", _start_node_ptr->label, "\"");
				else
					log("Benchmark result");

				double const passed_time_sec { (double)(stop_time_us - _start_time.value) / (double)(1000 * 1000) };
				log("   Ran ", passed_time_sec, " seconds.");

				if (_num_virt_blks_read) {

					size_t const bytes_read { _num_virt_blks_read * Tresor::BLOCK_SIZE };
					double const mibyte_read { (double)bytes_read / (double)(1024 * 1024) };
					double const mibyte_per_sec_read {
						(double)bytes_read / (double)passed_time_sec / (double)(1024 * 1024) };

					log("   Have read ", mibyte_read, " mebibyte in total.");
					log("   Have read ", mibyte_per_sec_read, " mebibyte per second.");
				}

				if (_num_virt_blks_written) {

					size_t bytes_written { _num_virt_blks_written * Tresor::BLOCK_SIZE };
					double const mibyte_written { (double)bytes_written / (double)(1024 * 1024) };
					double const mibyte_per_sec_written {
						(double)bytes_written / (double)passed_time_sec / (double)(1024 * 1024) };

					log("   Have written ", mibyte_written, " mebibyte in total.");
					log("   Have written ", mibyte_per_sec_written, " mebibyte per second.");
				}
				log("");
				_state = STOPPED;
				break;
			}
		}

		void raise_num_virt_blks_read() { _num_virt_blks_read++; }
		void raise_num_virt_blks_written() { _num_virt_blks_written++; }
};

struct Tresor_tester::Log_node : Noncopyable
{
	using String = Genode::String<128>;

	String const string;

	Log_node(Xml_node const &node) : string(node.attribute_value("string", String())) { }
};

struct Tresor_tester::Initialize_trust_anchor_node : Noncopyable
{
	Passphrase const passphrase;

	Initialize_trust_anchor_node(Xml_node const &node)
	:
		passphrase(node.attribute_value("passphrase", Passphrase()))
	{ }
};

struct Tresor_tester::Request_node : Noncopyable
{
	using Operation = Tresor::Request::Operation;

	Operation const op;
	Virtual_block_address const vba;
	Number_of_blocks const count;
	bool const sync;
	bool const salt_avail;
	Salt const salt;
	Snapshot_id const snap_id;

	Operation read_op_attr(Xml_node const &node)
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

	Request_node(Xml_node const &node)
	:
		op(read_op_attr(node)),
		vba(has_vba() ? read_attribute<Virtual_block_address>(node, "vba") : 0),
		count(has_count() ? read_attribute<Number_of_blocks>(node, "count") : 0),
		sync(read_attribute<bool>(node, "sync")),
		salt_avail(has_salt() ? node.has_attribute("salt") : false),
		salt(has_salt() && salt_avail ? read_attribute<Salt>(node, "salt") : 0),
		snap_id(has_snap_id() ? read_attribute<Snapshot_id>(node, "id") : 0)
	{ }

	bool has_vba() const { return op == Operation::READ || op == Operation::WRITE || op == Operation::SYNC; }

	bool has_salt() const { return op == Operation::READ || op == Operation::WRITE; }

	bool has_count() const
	{
		return op == Operation::READ || op == Operation::WRITE || op == Operation::SYNC ||
			   op == Operation::EXTEND_FT || op == Operation::EXTEND_VBD;
	}

	bool has_snap_id() const { return op == Operation::DISCARD_SNAPSHOT || op == Operation::CREATE_SNAPSHOT; }
};

struct Tresor_tester::Command : Avl_node<Command>
{
	using Type_string = String<64>;

	enum Type {
		REQUEST, INIT_TRUST_ANCHOR, BENCHMARK, CONSTRUCT, DESTRUCT, INITIALIZE, CHECK,
		CHECK_SNAPSHOTS, LOG };

	enum State { INIT, IN_PROGRESS, COMPLETE };

	/*
	 * Noncopyable
	 */
	Command(Command const &) = delete;
	Command &operator = (Command const &) = delete;

	Type const type;
	Command_id const id { 0 };
	State state { INIT };
	Generation generation { 0 };
	Constructible<Request_node> request_node { };
	Constructible<Initialize_trust_anchor_node> init_trust_anchor_node { };
	Constructible<Benchmark_node> benchmark_node { };
	Constructible<Log_node> log_node { };
	Constructible<Tresor_init::Configuration> initialize_config { };
	Trust_anchor::Initialize *init_trust_anchor_ptr { };
	Sb_initializer::Initialize *init_superblocks_ptr { };
	Sb_check::Check *check_superblocks_ptr { };
	Request *request_ptr { };

	static char const *_type_to_string(Type type)
	{
		switch (type) {
		case INITIALIZE: return "initialize";
		case REQUEST: return "request";
		case INIT_TRUST_ANCHOR: return "init_trust_anchor";
		case BENCHMARK: return "benchmark";
		case CONSTRUCT: return "construct";
		case DESTRUCT: return "destruct";
		case CHECK: return "check";
		case CHECK_SNAPSHOTS: return "check_snapshots";
		case LOG: return "log";
		}
		ASSERT_NEVER_REACHED;
	}

	static Type _string_to_type(Type_string str)
	{
		if (str == "initialize") { return INITIALIZE; }
		if (str == "request") { return REQUEST; }
		if (str == "initialize-trust-anchor") { return INIT_TRUST_ANCHOR; }
		if (str == "benchmark") { return BENCHMARK; }
		if (str == "construct") { return CONSTRUCT; }
		if (str == "destruct") { return DESTRUCT; }
		if (str == "check") { return CHECK; }
		if (str == "check-snapshots") { return CHECK_SNAPSHOTS; }
		if (str == "log") { return LOG; }
		ASSERT_NEVER_REACHED;
	}

	bool higher(Command *other_ptr) { return other_ptr->id > id; }

	Command(Xml_node const &node, Command_id id)
	:
		type(_string_to_type(node.type())), id(id)
	{
		switch (type) {
		case INITIALIZE: initialize_config.construct(node); break;
		case REQUEST: request_node.construct(node); break;
		case INIT_TRUST_ANCHOR: init_trust_anchor_node.construct(node); break;
		case BENCHMARK: benchmark_node.construct(node); break;
		case LOG: log_node.construct(node); break;
		default: break;
		}
	}

	template <typename FUNC>
	void with_command(Command_id id, FUNC && func)
	{
		if (id != this->id) {
			Command *cmd_ptr { Avl_node<Command>::child(id > this->id) };
			ASSERT(cmd_ptr);
			cmd_ptr->with_command(id, func);
		} else
			func(*this);
	}

	void print(Genode::Output &out) const
	{
		Genode::print(out, "id ", id, " type ", _type_to_string(type));
		if (type == REQUEST)
			Genode::print(out, " ", *request_ptr);
	}
};

struct Tresor_tester::Snapshot_reference : Avl_node<Snapshot_reference>
{
	Snapshot_id const id;
	Generation const gen;

	Snapshot_reference(Snapshot_id id, Generation gen) : id(id), gen(gen) { }

	template <typename FUNC>
	void with_ref(Snapshot_id target_id, FUNC && func) const
	{
		if (target_id != id) {
			Snapshot_reference *child_ptr { Avl_node<Snapshot_reference>::child(target_id > id) };
			if (child_ptr)
				child_ptr->with_ref(target_id, func);
			else
				ASSERT_NEVER_REACHED;
		} else
			func(*this);
	}

	void print(Genode::Output &out) const { Genode::print(out, "id ", id, " gen ", gen); }

	bool higher(Snapshot_reference *other_ptr) { return other_ptr->id > id; }
};

struct Tresor_tester::Snapshot_reference_tree : public Avl_tree<Snapshot_reference>
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

struct Tresor_tester::Crypto_key
{
	Key_id const key_id;
	Vfs::Vfs_handle &encrypt_file;
	Vfs::Vfs_handle &decrypt_file;
};

class Tresor_tester::Main : Vfs::Env::User, public Client_data_interface, public Crypto_key_files_interface
{
	private:

		Genode::Env &_env;
		Attached_rom_dataspace _config_rom { _env, "config" };
		Avl_tree<Command> _commands { };
		Tresor::Path const _crypto_path { _config_rom.xml().sub_node("crypto").attribute_value("path", Tresor::Path()) };
		Tresor::Path const _block_io_path { _config_rom.xml().sub_node("block-io").attribute_value("path", Tresor::Path()) };
		Tresor::Path const _trust_anchor_path { _config_rom.xml().sub_node("trust-anchor").attribute_value("path", Tresor::Path()) };
		Heap _heap { _env.ram(), _env.rm() };
		Vfs::Simple_env _vfs_env { _env, _heap, _config_rom.xml().sub_node("vfs"), *this };
		Vfs::Vfs_handle &_block_io_file { open_file(_vfs_env, _block_io_path, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_crypto_add_key_file { open_file(_vfs_env, { _crypto_path, "/add_key" }, Vfs::Directory_service::OPEN_MODE_WRONLY) };
		Vfs::Vfs_handle &_crypto_remove_key_file { open_file(_vfs_env, { _crypto_path, "/remove_key" }, Vfs::Directory_service::OPEN_MODE_WRONLY) };
		Vfs::Vfs_handle &_ta_decrypt_file { open_file(_vfs_env, { _trust_anchor_path, "/decrypt" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_ta_encrypt_file { open_file(_vfs_env, { _trust_anchor_path, "/encrypt" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_ta_generate_key_file { open_file(_vfs_env, { _trust_anchor_path, "/generate_key" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_ta_initialize_file { open_file(_vfs_env, { _trust_anchor_path, "/initialize" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Vfs::Vfs_handle &_ta_hash_file { open_file(_vfs_env, { _trust_anchor_path, "/hash" }, Vfs::Directory_service::OPEN_MODE_RDWR) };
		Signal_handler<Main> _signal_handler { _env.ep(), *this, &Main::_handle_signal };
		Benchmark _benchmark { _env };
		unsigned long _num_errors { 0 };
		Snapshot_reference_tree _snap_refs { };
		Constructible<Free_tree> _free_tree { };
		Constructible<Virtual_block_device> _vbd { };
		Constructible<Superblock_control> _sb_control { };
		Constructible<Initializing_request_scheduler> _request_scheduler { };
		Constructible<Meta_tree> _meta_tree { };
		Trust_anchor _trust_anchor { { _ta_decrypt_file, _ta_encrypt_file, _ta_generate_key_file, _ta_initialize_file, _ta_hash_file } };
		Crypto _crypto { {*this, _crypto_add_key_file, _crypto_remove_key_file} };
		Block_io _block_io { _block_io_file };
		Pba_allocator _pba_alloc { NR_OF_SUPERBLOCK_SLOTS };
		Vbd_initializer _vbd_initializer { };
		Ft_initializer _ft_initializer { };
		Sb_initializer _sb_initializer { };
		Vbd_check _vbd_check { };
		Ft_check _ft_check { };
		Sb_check _sb_check { };
		Constructible<Crypto_key> _crypto_keys[2] { };

		static void _generate_blk_data(Tresor::Block &blk_data, Virtual_block_address vba, Salt salt)
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

		Constructible<Crypto_key> &_crypto_key(Key_id key_id)
		{
			for (Constructible<Crypto_key> &key : _crypto_keys)
				if (key.constructed() && key->key_id == key_id)
					return key;
			ASSERT_NEVER_REACHED;
		}

		template <typename FUNC>
		void _for_each_command(FUNC && func)
		{
			_commands.for_each([&] (Command const &cmd) {
				func(*const_cast<Command *>(&cmd)); });
		}

		void _wakeup_back_end_services() { _vfs_env.io().commit(); }

		bool _synchronized_command(Command const &cmd)
		{
			if (cmd.type == Command::REQUEST)
				return cmd.request_node->sync;

			return false;
		}

		void _remove_snap_ref(Snapshot_reference &ref)
		{
			_snap_refs.remove(&ref);
			ref.~Snapshot_reference();
			destroy(_heap, &ref);
		}

		void _reset_snap_refs()
		{
			while (_snap_refs.first())
				_remove_snap_ref(*_snap_refs.first());
		}

		void _remove_snap_refs_with_same_gen(Snapshot_id id)
		{
			Generation gen { _snap_id_to_gen(id) };
			while (1) {
				Snapshot_reference *ref_ptr { nullptr };
				_snap_refs.for_each([&] (Snapshot_reference const &ref) {
					if (!ref_ptr && ref.gen == gen)
						ref_ptr = const_cast<Snapshot_reference *>(&ref);
				});
				if (ref_ptr)
					_remove_snap_ref(*ref_ptr);
				else
					break;
			}
		}

		void _mark_command_in_progress(Command &cmd)
		{
			cmd.state = Command::IN_PROGRESS;
			if (VERBOSE)
				log("start command: ", cmd);
		}

		void _mark_command_complete(Command &cmd, bool success)
		{
			cmd.state = Command::COMPLETE;
			if (VERBOSE)
				log("finish command: ", cmd);
			if (!success) {
				_num_errors++;
				error("command failed: ", cmd);
			}
		}

		void _start_command(Command &cmd)
		{
			switch (cmd.type) {
			case Command::INIT_TRUST_ANCHOR:
			{
				Initialize_trust_anchor_node &node { *cmd.init_trust_anchor_node };
				cmd.init_trust_anchor_ptr = new (_heap) Trust_anchor::Initialize({node.passphrase});
				_mark_command_in_progress(cmd);
				break;
			}
			case Command::INITIALIZE:
			{
				_reset_snap_refs();
				Tresor_init::Configuration const &cfg { *cmd.initialize_config };
				cmd.init_superblocks_ptr = new (_heap) Sb_initializer::Initialize({
					Tree_configuration {
						(Tree_level_index)(cfg.vbd_nr_of_lvls() - 1),
						(Tree_degree)cfg.vbd_nr_of_children(),
						cfg.vbd_nr_of_leafs()
					},
					Tree_configuration {
						(Tree_level_index)cfg.ft_nr_of_lvls() - 1,
						(Tree_degree)cfg.ft_nr_of_children(),
						cfg.ft_nr_of_leafs()
					},
					Tree_configuration {
						(Tree_level_index)cfg.ft_nr_of_lvls() - 1,
						(Tree_degree)cfg.ft_nr_of_children(),
						cfg.ft_nr_of_leafs()
					},
					_pba_alloc
				});
				_mark_command_in_progress(cmd);
				break;
			}
			case Command::CHECK:

				cmd.check_superblocks_ptr = new (_heap) Sb_check::Check();
				_mark_command_in_progress(cmd);
				break;

			case Command::REQUEST:
			{
				Request_node const &node { *cmd.request_node };
				cmd.generation = node.op == Request::DISCARD_SNAPSHOT ? _snap_id_to_gen(node.snap_id) : 0;
				cmd.request_ptr = new (_heap) Request(
					node.op, node.has_vba() ? node.vba : 0, 0, node.has_count() ? node.count : 0,
					cmd.id, cmd.generation);

				_request_scheduler->add_request(*cmd.request_ptr);
				_mark_command_in_progress(cmd);
				break;
			}
			case Command::BENCHMARK:
			case Command::CONSTRUCT:
			case Command::DESTRUCT:
			case Command::CHECK_SNAPSHOTS:
			case Command::LOG: _mark_command_in_progress(cmd); break;
			default: ASSERT_NEVER_REACHED;
			}
		}

		template <typename REQUEST>
		bool _try_complete_command(Command &cmd, REQUEST &req, bool &progress)
		{
			if (!req.complete())
				return false;

			_mark_command_complete(cmd, req.success());
			destroy(_heap, &req);
			progress = true;
			return true;
		}

		bool _execute_command(Command &cmd, bool &cmd_complete)
		{
			cmd_complete = false;
			bool progress = false;
			switch (cmd.type) {
			case Command::INIT_TRUST_ANCHOR:
			{
				Trust_anchor::Initialize &req = *cmd.init_trust_anchor_ptr;
				progress |= _trust_anchor.execute(req);
				cmd_complete = _try_complete_command(cmd, req, progress);
				break;
			}
			case Command::INITIALIZE:
			{
				Sb_initializer::Initialize &req = *cmd.init_superblocks_ptr;
				progress |= _sb_initializer.execute(req, _block_io, _trust_anchor, _vbd_initializer, _ft_initializer);
				cmd_complete = _try_complete_command(cmd, req, progress);
				break;
			}
			case Command::CONSTRUCT:

				_meta_tree.construct();
				_free_tree.construct();
				_vbd.construct();
				_sb_control.construct();
				_request_scheduler.construct();
				_mark_command_complete(cmd, true);
				cmd_complete = true;
				progress = true;
				break;

			case Command::DESTRUCT:

				_meta_tree.destruct();
				_free_tree.destruct();
				_vbd.destruct();
				_sb_control.destruct();
				_request_scheduler.destruct();
				_mark_command_complete(cmd, true);
				cmd_complete = true;
				progress = true;
				break;

			case Command::BENCHMARK:

				_benchmark.execute_cmd(*cmd.benchmark_node);
				_mark_command_complete(cmd, true);
				cmd_complete = true;
				progress = true;
				break;

			case Command::CHECK_SNAPSHOTS:
			{
				bool success { true };
				Snapshots_info snap_info { _sb_control->snapshots_info() };
				bool snap_gen_ok[MAX_NR_OF_SNAPSHOTS] { false };
				_snap_refs.for_each([&] (Snapshot_reference const &snap_ref) {
					bool snap_ref_ok { false };
					for (Snapshot_index idx { 0 }; idx < MAX_NR_OF_SNAPSHOTS; idx++) {
						if (snap_info.generations[idx] == snap_ref.gen) {
							snap_ref_ok = true;
							snap_gen_ok[idx] = true;
						}
					}
					if (!snap_ref_ok) {
						warning("snap (", snap_ref, ") not known to tresor");
						success = false;
					}
				});
				for (Snapshot_index idx { 0 }; idx < MAX_NR_OF_SNAPSHOTS; idx++) {
					if (snap_info.generations[idx] != INVALID_GENERATION && !snap_gen_ok[idx]) {
						warning("snap (idx ", idx, " gen ", snap_info.generations[idx], ") not known to tester");
						success = false;
					}
				}
				_mark_command_complete(cmd, success);
				progress = true;
				break;
			}
			case Command::REQUEST:
			{
				Request &req = *cmd.request_ptr;
				Request_node &node = *cmd.request_node;
				progress |= _request_scheduler->execute({*_sb_control, *this, *_vbd, *_free_tree, *_meta_tree, _block_io, _trust_anchor, _crypto });
				if (req.complete() && req.success()) {
					switch (req.op()) {
					case Request::CREATE_SNAPSHOT: _snap_refs.insert(new (_heap) Snapshot_reference { node.snap_id, cmd.generation }); break;
					case Request::DISCARD_SNAPSHOT: _remove_snap_refs_with_same_gen(node.snap_id); break;
					default: break;
					}
				}
				cmd_complete = _try_complete_command(cmd, req, progress);
				break;
			}
			case Command::LOG:

				log("\n", cmd.log_node->string, "\n");
				_mark_command_complete(cmd, true);
				cmd_complete = true;
				progress = true;
				break;

			case Command::CHECK:
			{
				Sb_check::Check &req = *cmd.check_superblocks_ptr;
				progress |= _sb_check.execute(req, _vbd_check, _ft_check, _block_io);
				cmd_complete = _try_complete_command(cmd, req, progress);
				break;
			}
			default: ASSERT_NEVER_REACHED;
			}
			return progress;
		}

		bool _execute_commands(bool &all_cmds_complete)
		{
			all_cmds_complete = true;
			bool cmds_in_progress = false;
			bool progress = false;
			bool ignore_remaining_cmds = false;
			Command *last_cmd_ptr { };
			_for_each_command([&] (Command &cmd)
			{
				/*
				 * Commands that are processed by different top-level modules
				 * (tresor request scheduler, tresor initializer,
				 * tresor check, trust anchor) must always be serialized.
				 */
				if (cmds_in_progress && last_cmd_ptr->type != cmd.type)
					ignore_remaining_cmds = true;

				if (ignore_remaining_cmds)
					return;

				switch (cmd.state) {
				case Command::INIT:

					all_cmds_complete = false;
					if (_synchronized_command(cmd) && cmds_in_progress) {
						ignore_remaining_cmds = true;
						break;
					}
					_start_command(cmd);
					progress = true;
					cmds_in_progress = true;
					break;

				case Command::IN_PROGRESS:

					bool cmd_complete;
					progress |= _execute_command(cmd, cmd_complete);
					if (!cmd_complete) {
						all_cmds_complete = false;
						cmds_in_progress = true;
					}
					break;

				default: break;
				}
				last_cmd_ptr = &cmd;
			});
			return progress;
		}

		void _handle_signal()
		{
			bool all_cmds_complete;
			while (_execute_commands(all_cmds_complete));
			if (all_cmds_complete) {
				if (_num_errors) {
					error(_num_errors, " command", _num_errors > 1 ? "s" : "", " failed!");
					_env.parent().exit(-1);
				} else {
					log("All commands succeeded!");
					_env.parent().exit(0);
				}
			}
			_wakeup_back_end_services();
		}

		template <typename FUNC>
		void _with_command(Command_id id, FUNC && func)
		{
			ASSERT(_commands.first());
			_commands.first()->with_command(id, func);
		}

		Generation _snap_id_to_gen(Snapshot_id id)
		{
			Generation gen { INVALID_GENERATION };
			_snap_refs.with_ref(id, [&] (Snapshot_reference const &ref) {
				gen = ref.gen; });

			return gen;
		}

		/********************
		 ** Vfs::Env::User **
		 ********************/

		void wakeup_vfs_user() override { _signal_handler.local_submit(); }

		/********************************
		 ** Crypto_key_files_interface **
		 ********************************/

		void add_crypto_key(Key_id key_id) override
		{
			for (Constructible<Crypto_key> &key : _crypto_keys)
				if (!key.constructed()) {
					key.construct(key_id,
						open_file(_vfs_env, { _crypto_path, "/keys/", key_id, "/encrypt" }, Vfs::Directory_service::OPEN_MODE_RDWR),
						open_file(_vfs_env, { _crypto_path, "/keys/", key_id, "/decrypt" }, Vfs::Directory_service::OPEN_MODE_RDWR)
					);
					return;
				}
			ASSERT_NEVER_REACHED;
		}

		void remove_crypto_key(Key_id key_id) override
		{
			Constructible<Crypto_key> &crypto_key = _crypto_key(key_id);
			_vfs_env.root_dir().close(&crypto_key->encrypt_file);
			_vfs_env.root_dir().close(&crypto_key->decrypt_file);
			crypto_key.destruct();
		}

		Vfs::Vfs_handle &encrypt_file(Key_id key_id) override { return _crypto_key(key_id)->encrypt_file; }
		Vfs::Vfs_handle &decrypt_file(Key_id key_id) override { return _crypto_key(key_id)->decrypt_file; }

		/***************************
		 ** Client_data_interface **
		 ***************************/

		void obtain_data(Obtain_data_attr const &attr) override
		{
			_with_command(attr.in_req_tag, [&] (Command &cmd) {
				ASSERT(cmd.type == Command::REQUEST);
				Request_node const &node { *cmd.request_node };
				if (node.salt_avail)
					_generate_blk_data(attr.out_blk, attr.in_vba, node.salt);
			});
			_benchmark.raise_num_virt_blks_written();
		}

		void supply_data(Supply_data_attr const &attr) override
		{
			_with_command(attr.in_req_tag, [&] (Command &cmd) {
				ASSERT(cmd.type == Command::REQUEST);
				Request_node const &node { *cmd.request_node };
				if (node.salt_avail) {
					Tresor::Block gen_blk_data { };
					_generate_blk_data(gen_blk_data, attr.in_vba, node.salt);

					if (memcmp(&attr.in_blk, &gen_blk_data, BLOCK_SIZE)) {
						warning("client data mismatch: vba=", attr.in_vba, " req_tag=", attr.in_req_tag);
						_num_errors++;
					}
				}
			});
			_benchmark.raise_num_virt_blks_read();
		}

	public:

		Main(Genode::Env &env) : _env(env)
		{
			Command_id command_id { 0 };
			_config_rom.xml().sub_node("commands").for_each_sub_node([&] (Xml_node const &node) {
				_commands.insert(new (_heap) Command(node, command_id++));
			});
			_handle_signal();
		}
};


void Component::construct(Genode::Env &env) { static Tresor_tester::Main main(env); }


namespace Libc {

	struct Env;
	struct Component { void construct(Libc::Env &) { } };
}
