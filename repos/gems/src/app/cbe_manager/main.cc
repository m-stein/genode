/*
 * \brief  Graphical front end for controlling CBE devices
 * \author Martin Stein
 * \author Norman Feske
 * \date   2021-02-24
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/session_object.h>
#include <base/attached_rom_dataspace.h>
#include <base/buffered_output.h>
#include <os/buffered_xml.h>
#include <os/sandbox.h>
#include <os/dynamic_rom_session.h>
#include <os/vfs.h>
#include <os/reporter.h>

/* local includes */
#include <gui_session_component.h>
#include <report_session_component.h>
#include <new_file.h>
#include <child_state.h>
#include <sandbox.h>
#include <input.h>
#include <utf8.h>
#include <child_exit_state.h>
#include <menu_view_dialog.h>
#include <const_pointer.h>
#include <snapshot.h>

namespace Cbe_manager {

	class Main;
}

class Cbe_manager::Main
:
	private Sandbox::Local_service_base::Wakeup,
	private Sandbox::State_handler,
	private Gui::Input_event_handler,
	private Dynamic_rom_session::Xml_producer
{
	private:

		enum {
			STATE_STRING_CAPACITY = 3,
			CBE_BLOCK_SIZE = 4096,
			MAIN_FRAME_WIDTH = 40,
			INIT_CBE_NR_OF_LEVELS = 6,
			INIT_CBE_NR_OF_CHILDREN = 64,
			INIT_CBE_NR_OF_SUPERBLOCKS = 8,
		};

		enum class State
		{
			INVALID,
			SETUP_OBTAIN_PARAMETERS,
			SETUP_CREATE_CBE_IMAGE_FILE,
			SETUP_RUN_CBE_INIT_TRUST_ANCHOR,
			SETUP_RUN_CBE_INIT,
			SETUP_START_CBE_VFS,
			SETUP_FORMAT_CBE,
			STARTUP_OBTAIN_PARAMETERS,
			STARTUP_RUN_CBE_INIT_TRUST_ANCHOR,
			STARTUP_START_CBE_VFS,
			CONTROLS_ROOT,
			CONTROLS_SNAPSHOTS,
			CONTROLS_DIMENSIONS,
			CONTROLS_SECURITY,
			CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY,
			CONTROLS_SECURITY_MASTER_KEY,
			CONTROLS_SECURITY_USER_PASSPHRASE,
			SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE,
			SHUTDOWN_WAIT_TILL_DEINIT_REQUEST_IS_DONE
		};

		enum class Setup_obtain_params_hover
		{
			NONE,
			PASSPHRASE_1_INPUT,
			PASSPHRASE_2_INPUT,
			SIZE_INPUT,
			START_BUTTON
		};

		enum class Setup_obtain_params_select
		{
			NONE,
			PASSPHRASE_1_INPUT,
			PASSPHRASE_2_INPUT,
			SIZE_INPUT,
			START_BUTTON
		};

		enum class Controls_root_select
		{
			NONE,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_root_hover
		{
			NONE,
			SNAPSHOTS_EXPAND_BUTTON,
			DIMENSIONS_EXPAND_BUTTON,
			SECURITY_EXPAND_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_snapshots_select
		{
			NONE,
			SHUT_DOWN_BUTTON,
			CREATE_SNAPSHOT_BUTTON,
			DISCARD_SNAPSHOT_BUTTON,
		};

		enum class Controls_snapshots_hover
		{
			NONE,
			SNAPSHOTS_EXPAND_BUTTON,
			SHUT_DOWN_BUTTON,
			CREATE_SNAPSHOT_BUTTON,
			DISCARD_SNAPSHOT_BUTTON,
		};

		enum class Controls_dimensions_select
		{
			NONE,
			RESIZING_NR_OF_BLKS_INPUT,
			RESIZING_START_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_dimensions_hover
		{
			NONE,
			DIMENSIONS_EXPAND_BUTTON,
			RESIZING_NR_OF_BLKS_INPUT,
			RESIZING_START_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_security_block_encryption_key_select
		{
			NONE,
			REPLACE_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_security_block_encryption_key_hover
		{
			NONE,
			LEAVE_BUTTON,
			REPLACE_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_security_master_key_select
		{
			NONE,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_security_master_key_hover
		{
			NONE,
			LEAVE_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_security_user_passphrase_select
		{
			NONE,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_security_user_passphrase_hover
		{
			NONE,
			LEAVE_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_security_select
		{
			NONE,
			BLOCK_ENCRYPTION_KEY_EXPAND_BUTTON,
			MASTER_KEY_EXPAND_BUTTON,
			USER_PASSPHRASE_EXPAND_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Controls_security_hover
		{
			NONE,
			SECURITY_EXPAND_BUTTON,
			BLOCK_ENCRYPTION_KEY_EXPAND_BUTTON,
			MASTER_KEY_EXPAND_BUTTON,
			USER_PASSPHRASE_EXPAND_BUTTON,
			SHUT_DOWN_BUTTON,
		};

		enum class Resizing_state
		{
			INACTIVE,
			WAIT_TILL_DEVICE_IS_READY,
			ISSUE_REQUEST_AT_DEVICE,
			IN_PROGRESS_AT_DEVICE,
		};

		enum class Rekeying_state
		{
			INACTIVE,
			WAIT_TILL_DEVICE_IS_READY,
			ISSUE_REQUEST_AT_DEVICE,
			IN_PROGRESS_AT_DEVICE,
		};

		enum class Create_snapshot_state
		{
			INACTIVE,
			ISSUE_REQUEST_AT_DEVICE,
		};

		enum class Discard_snapshot_state
		{
			INACTIVE,
			ISSUE_REQUEST_AT_DEVICE,
		};

		using Report_service     = Sandbox::Local_service<Report::Session_component>;
		using Gui_service        = Sandbox::Local_service<Gui::Session_component>;
		using Rom_service        = Sandbox::Local_service<Dynamic_rom_session>;
		using Xml_report_handler = Report::Session_component::Xml_handler<Main>;
		using State_string       = String<STATE_STRING_CAPACITY>;
		using Snapshot_registry  = Registry<Registered<Snapshot>>;
		using Snapshot_pointer   = Const_pointer<Snapshot>;

		Env                                   &_env;
		static constexpr char           const *_setup_title                        { "Setup" };
		static constexpr char           const *_shutdown_title                     { "Shutdown" };
		static constexpr char           const *_controls_title                     { "Controls" };
		static constexpr char           const *_startup_title                      { "Startup" };
		State                                  _state                              { State::INVALID };
		Heap                                   _heap                               { _env.ram(), _env.rm() };
		Attached_rom_dataspace                 _config                             { _env, "config" };
		Root_directory                         _vfs                                { _env, _heap, _config.xml().sub_node("vfs") };
		Registry<Child_state>                  _children                           { };
		Child_state                            _menu_view                          { _children, "menu_view", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _mke2fs                             { _children, "mke2fs", Ram_quota { 100 * 1024 * 1024 }, Cap_quota { 500 } };
		Child_state                            _cbe_vfs                            { _children, "cbe_vfs", "vfs", Ram_quota { 64 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _cbe_trust_anchor_vfs               { _children, "cbe_trust_anchor_vfs", "vfs", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _rump_vfs                           { _children, "rump_vfs", "vfs", Ram_quota { 16 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _sync_to_cbe_vfs_init               { _children, "sync_to_cbe_vfs_init", "cbe_manager-sync_to_cbe_vfs_init", Ram_quota { 8 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _cbe_vfs_block                      { _children, "vfs_block", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _fs_query                           { _children, "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _cbe_init_trust_anchor              { _children, "cbe_init_trust_anchor", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _cbe_image_vfs_block                { _children, "vfs_block", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _cbe_init                           { _children, "cbe_init", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _snapshots_fs_query                 { _children, "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _resizing_fs_tool                   { _children, "resizing_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _resizing_fs_query                  { _children, "resizing_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _rekeying_fs_tool                   { _children, "rekeying_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _rekeying_fs_query                  { _children, "rekeying_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _shut_down_fs_tool                  { _children, "shut_down_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _shut_down_fs_query                 { _children, "shut_down_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _create_snap_fs_tool                { _children, "create_snap_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _discard_snap_fs_tool               { _children, "discard_snap_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Xml_report_handler                     _fs_query_listing_handler           { *this, &Main::_handle_fs_query_listing };
		Xml_report_handler                     _resizing_fs_query_listing_handler  { *this, &Main::_handle_resizing_fs_query_listing };
		Xml_report_handler                     _rekeying_fs_query_listing_handler  { *this, &Main::_handle_rekeying_fs_query_listing };
		Xml_report_handler                     _shut_down_fs_query_listing_handler { *this, &Main::_handle_shut_down_fs_query_listing };
		Sandbox                                _sandbox                            { _env, *this };
		Gui_service                            _gui_service                        { _sandbox, *this };
		Rom_service                            _rom_service                        { _sandbox, *this };
		Report_service                         _report_service                     { _sandbox, *this };
		Xml_report_handler                     _hover_handler                      { *this, &Main::_handle_hover };
		Constructible<Watch_handler<Main>>     _watch_handler                      { };
		Constructible<Expanding_reporter>      _clipboard_reporter                 { };
		Constructible<Attached_rom_dataspace>  _clipboard_rom                      { };
		bool                                   _initial_config                     { true };
		Signal_handler<Main>                   _config_handler                     { _env.ep(), *this, &Main::_handle_config };
		Signal_handler<Main>                   _state_handler                      { _env.ep(), *this, &Main::_handle_state };
		Dynamic_rom_session                    _dialog                             { _env.ep(), _env.ram(), _env.rm(), *this };
		Input_passphrase                       _setup_obtain_params_passphrase_1   { };
		Input_passphrase                       _setup_obtain_params_passphrase_2   { };
		Input_number_of_bytes                  _setup_obtain_params_size           { };
		Setup_obtain_params_hover              _setup_obtain_params_hover          { Setup_obtain_params_hover::NONE };
		Setup_obtain_params_select             _setup_obtain_params_select         { Setup_obtain_params_select::PASSPHRASE_1_INPUT };
		Controls_root_hover                    _controls_root_hover                { Controls_root_select::NONE };
		Controls_root_select                   _controls_root_select               { Controls_root_hover::NONE };
		Controls_snapshots_hover               _controls_snapshots_hover           { Controls_snapshots_select::NONE };
		Controls_snapshots_select              _controls_snapshots_select          { Controls_snapshots_hover::NONE };
		Controls_dimensions_hover              _controls_dimensions_hover          { Controls_dimensions_select::NONE };
		Controls_dimensions_select             _controls_dimensions_select         { Controls_dimensions_hover::NONE };
		Controls_security_hover                _controls_security_hover            { Controls_security_select::NONE };
		Controls_security_select               _controls_security_select           { Controls_security_hover::NONE };

		Controls_security_master_key_hover             _controls_security_master_key_hover            { Controls_security_master_key_select::NONE };
		Controls_security_master_key_select            _controls_security_master_key_select           { Controls_security_master_key_hover::NONE };
		Controls_security_block_encryption_key_hover   _controls_security_block_encryption_key_hover  { Controls_security_block_encryption_key_select::NONE };
		Controls_security_block_encryption_key_select  _controls_security_block_encryption_key_select { Controls_security_block_encryption_key_hover::NONE };
		Controls_security_user_passphrase_hover        _controls_security_user_passphrase_hover       { Controls_security_user_passphrase_select::NONE };
		Controls_security_user_passphrase_select       _controls_security_user_passphrase_select      { Controls_security_user_passphrase_hover::NONE };

		Resizing_state                         _resizing_state                     { Resizing_state::INACTIVE };
		Input_number_of_blocks                 _resizing_nr_of_blks                { };
		Rekeying_state                         _rekeying_state                     { Rekeying_state::INACTIVE };
		Create_snapshot_state                  _create_snap_state                  { Create_snapshot_state::INACTIVE };
		Discard_snapshot_state                 _discard_snap_state                 { Discard_snapshot_state::INACTIVE };
		Generation                             _discard_snap_gen                   { INVALID_GENERATION };
		Snapshot_registry                      _snapshots                          { };
		Snapshot_pointer                       _snapshots_hover                    { };
		Snapshot_pointer                       _snapshots_select                   { };
		bool                                   _snapshots_expanded                 { false };
		bool                                   _dimensions_expanded                { false };
		bool                                   _startup_failed                     { false };

		template <typename FUNCTOR>
		static void _if_child_exited(Xml_node    const &sandbox_state,
		                             Child_state const &child_state,
		                             FUNCTOR     const &functor)
		{
			Child_exit_state const exit_state { sandbox_state, child_state.start_name() };

			if (!exit_state.exists()) {
				class Child_doesnt_exist { };
				throw Child_doesnt_exist { };
			}
			if (exit_state.exited()) {

				functor(exit_state.code());
			}
		}

		static bool _child_succeeded(Xml_node    const &sandbox_state,
		                            Child_state const &child_state);

		static State _state_from_string(State_string const &str);

		static State_string _state_to_string(State state);

		static State _state_from_fs_query_listing(Xml_node const &node);

		void _write_to_state_file(State state);

		static void _vfs_create_zero_filled_file(Root_directory  &vfs,
		                                         Allocator       &alloc,
		                                         Directory::Path  path,
		                                         size_t           blk_size,
		                                         size_t           nr_of_blks);

		void _generate_sandbox_config(Xml_generator &xml) const;

		void _handle_fs_query_listing(Xml_node const &node);

		void _handle_resizing_fs_query_listing(Xml_node const &node);

		void _handle_rekeying_fs_query_listing(Xml_node const &node);

		void _handle_shut_down_fs_query_listing(Xml_node const &node);

		void _handle_hover(Xml_node const &node);

		void _handle_config();

		void _handle_state();

		void _update_sandbox_config();

		size_t _init_cbe_nr_of_leafs() const;


		static size_t _tree_nr_of_blocks(size_t nr_of_lvls,
		                          size_t nr_of_children,
		                          size_t nr_of_leafs);

		Number_of_bytes _cbe_size() const;

		static size_t _cbe_nr_of_blocks(size_t nr_of_superblocks,
		                                size_t nr_of_vbd_lvls,
		                                size_t nr_of_vbd_children,
		                                size_t nr_of_vbd_leafs,
		                                size_t nr_of_ft_lvls,
		                                size_t nr_of_ft_children,
		                                size_t nr_of_ft_leafs);

		static bool cbe_control_file_yields_state_idle(Xml_node const &fs_query_listing,
		                                               char     const *file_name);


		/***************************************************
		 ** Sandbox::Local_service_base::Wakeup interface **
		 ***************************************************/

		void wakeup_local_service() override;


		/****************************
		 ** Sandbox::State_handler **
		 ****************************/

		void handle_sandbox_state() override;


		/****************************************
		 ** Gui::Input_event_handler interface **
		 ****************************************/

		void handle_input_event(Input::Event const &event) override;


		/***************************************
		 ** Dynamic_rom_session::Xml_producer **
		 ***************************************/

		void produce_xml(Xml_generator &xml) override;

	public:

		Main(Env &env);
};

using namespace Cbe_manager;


/***********************
 ** Cbe_manager::Main **
 ***********************/

void Main::_handle_config()
{
	_config.update();
	_initial_config = false;
}


bool Main::cbe_control_file_yields_state_idle(Xml_node const &fs_query_listing,
                                              char     const *file_name)
{
	bool result { false };
	bool done   { false };
	fs_query_listing.with_sub_node("dir", [&] (Xml_node const &node_0) {
		node_0.for_each_sub_node("file", [&] (Xml_node const &node_1) {
			if (done) {
				return;
			}
			if (node_1.attribute_value("name", String<16>()) == file_name) {
				node_1.with_raw_content([&] (char const *base, size_t size) {
					result = String<5> { Cstring {base, size} } == "idle";
					done = true;
				});
			}
		});
	});
	return result;
}


void Main::_update_sandbox_config()
{
	Buffered_xml const config { _heap, "config", [&] (Xml_generator &xml) {
		_generate_sandbox_config(xml); } };

	config.with_xml_node([&] (Xml_node const &config) {
		_sandbox.apply_config(config); });
}


Main::State Main::_state_from_string(State_string const &str)
{
	if (str ==  "0") { return State::INVALID; }
	if (str ==  "1") { return State::SETUP_OBTAIN_PARAMETERS; }
	if (str ==  "2") { return State::SETUP_RUN_CBE_INIT_TRUST_ANCHOR; }
	if (str ==  "3") { return State::SETUP_CREATE_CBE_IMAGE_FILE; }
	if (str ==  "4") { return State::SETUP_RUN_CBE_INIT; }
	if (str ==  "5") { return State::SETUP_START_CBE_VFS; }
	if (str ==  "6") { return State::SETUP_FORMAT_CBE; }
	if (str ==  "7") { return State::CONTROLS_ROOT; }
	if (str ==  "8") { return State::CONTROLS_SNAPSHOTS; }
	if (str ==  "9") { return State::CONTROLS_DIMENSIONS; }
	if (str == "10") { return State::CONTROLS_SECURITY; }
	if (str == "11") { return State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY; }
	if (str == "12") { return State::CONTROLS_SECURITY_MASTER_KEY; }
	if (str == "13") { return State::CONTROLS_SECURITY_USER_PASSPHRASE; }
	if (str == "14") { return State::STARTUP_OBTAIN_PARAMETERS; }
	if (str == "15") { return State::STARTUP_RUN_CBE_INIT_TRUST_ANCHOR; }
	if (str == "16") { return State::STARTUP_START_CBE_VFS; }
	if (str == "17") { return State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE; }
	if (str == "18") { return State::SHUTDOWN_WAIT_TILL_DEINIT_REQUEST_IS_DONE; }
	class Invalid_state_string { };
	throw Invalid_state_string { };
}


Main::State_string Main::_state_to_string(State state)
{
	switch (state) {
	case State::INVALID:                                   return  "0";
	case State::SETUP_OBTAIN_PARAMETERS:                   return  "1";
	case State::SETUP_RUN_CBE_INIT_TRUST_ANCHOR:           return  "2";
	case State::SETUP_CREATE_CBE_IMAGE_FILE:               return  "3";
	case State::SETUP_RUN_CBE_INIT:                        return  "4";
	case State::SETUP_START_CBE_VFS:                       return  "5";
	case State::SETUP_FORMAT_CBE:                          return  "6";
	case State::CONTROLS_ROOT:                             return  "7";
	case State::CONTROLS_SNAPSHOTS:                        return  "8";
	case State::CONTROLS_DIMENSIONS:                       return  "9";
	case State::CONTROLS_SECURITY:                         return "10";
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:    return "11";
	case State::CONTROLS_SECURITY_MASTER_KEY:              return "12";
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:         return "13";
	case State::STARTUP_OBTAIN_PARAMETERS:                 return "14";
	case State::STARTUP_RUN_CBE_INIT_TRUST_ANCHOR:         return "15";
	case State::STARTUP_START_CBE_VFS:                     return "16";
	case State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE:      return "17";
	case State::SHUTDOWN_WAIT_TILL_DEINIT_REQUEST_IS_DONE: return "18";
	}
	class Invalid_state { };
	throw Invalid_state { };
}


Main::State Main::_state_from_fs_query_listing(Xml_node const &node)
{
	State state { State::INVALID };
	node.with_sub_node("dir", [&] (Xml_node const &node_0) {
		node_0.with_sub_node("file", [&] (Xml_node const &node_1) {
			if (node_1.attribute_value("name", String<6>()) == "state") {
				state = _state_from_string(
					node_1.decoded_content<State_string>());
			}
		});
	});
	return state;
}


void Main::_write_to_state_file(State state)
{
	bool write_error = false;
	try {
		New_file new_file(_vfs, Directory::Path("/cbe/cbe_manager/state"));
		auto write = [&] (char const *str)
		{
			switch (new_file.append(str, strlen(str))) {
			case New_file::Append_result::OK:

				break;

			case New_file::Append_result::WRITE_ERROR:

				write_error = true;
				break;
			}
		};
		Buffered_output<STATE_STRING_CAPACITY, decltype(write)> output(write);
		print(output, _state_to_string(state));
	}
	catch (New_file::Create_failed) {

		class Create_state_file_failed { };
		throw Create_state_file_failed { };
	}
	if (write_error) {

		class Write_state_file_failed { };
		throw Write_state_file_failed { };
	}
}


void Main::_vfs_create_zero_filled_file(Root_directory  &vfs,
                                        Allocator       &alloc,
                                        Directory::Path  path,
                                        size_t           blk_size,
                                        size_t           nr_of_blks)
{
	char *blk_buf { (char *)alloc.alloc(blk_size) };
	memset(blk_buf, 0, blk_size);
	New_file new_file { vfs, path };

	for(size_t blk_idx { 0 }; blk_idx < nr_of_blks; blk_idx++) {

		bool write_error { false };
		try {
			switch (new_file.append(blk_buf, blk_size)) {
			case New_file::Append_result::OK:

				break;

			case New_file::Append_result::WRITE_ERROR:

				write_error = true;
				break;
			}
		}
		catch (New_file::Create_failed) {

			alloc.free((void *)blk_buf, blk_size);
			class Create_cbe_image_file_failed { };
			throw Create_cbe_image_file_failed { };
		}
		if (write_error) {

			alloc.free((void *)blk_buf, blk_size);
			class Write_cbe_image_file_failed { };
			throw Write_cbe_image_file_failed { };
		}
	}
	alloc.free((void *)blk_buf, blk_size);
}


void Main::_handle_resizing_fs_query_listing(Xml_node const &node)
{
	switch (_state) {
	case State::CONTROLS_ROOT:
	case State::CONTROLS_SNAPSHOTS:
	case State::CONTROLS_DIMENSIONS:
	case State::CONTROLS_SECURITY:
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	case State::CONTROLS_SECURITY_MASTER_KEY:
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:

		switch (_resizing_state) {
		case Resizing_state::WAIT_TILL_DEVICE_IS_READY:

			if (cbe_control_file_yields_state_idle(node, "extend")) {

				_resizing_state = Resizing_state::ISSUE_REQUEST_AT_DEVICE;
				Signal_transmitter(_state_handler).submit();
			}
			break;

		case Resizing_state::IN_PROGRESS_AT_DEVICE:

			if (cbe_control_file_yields_state_idle(node, "extend")) {

				_resizing_nr_of_blks = Input_number_of_blocks { };
				_resizing_state = Resizing_state::INACTIVE;
				Signal_transmitter(_state_handler).submit();
			}
			break;

		default:

			break;
		}

	default:

		break;
	}
}


void Main::_handle_shut_down_fs_query_listing(Xml_node const &node)
{
	switch (_state) {
	case State::SHUTDOWN_WAIT_TILL_DEINIT_REQUEST_IS_DONE:

		if (cbe_control_file_yields_state_idle(node, "deinitialize")) {

			_env.parent().exit(0);
		}
		break;

	default:

		break;
	}
}


void Main::_handle_rekeying_fs_query_listing(Xml_node const &node)
{
	switch (_state) {
	case State::CONTROLS_ROOT:
	case State::CONTROLS_SNAPSHOTS:
	case State::CONTROLS_DIMENSIONS:
	case State::CONTROLS_SECURITY:
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	case State::CONTROLS_SECURITY_MASTER_KEY:
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:

		switch (_rekeying_state) {
		case Rekeying_state::WAIT_TILL_DEVICE_IS_READY:

			if (cbe_control_file_yields_state_idle(node, "rekey")) {

				_rekeying_state = Rekeying_state::ISSUE_REQUEST_AT_DEVICE;
				Signal_transmitter(_state_handler).submit();
			}
			break;

		case Rekeying_state::IN_PROGRESS_AT_DEVICE:

			if (cbe_control_file_yields_state_idle(node, "rekey")) {

				_rekeying_state = Rekeying_state::INACTIVE;
				Signal_transmitter(_state_handler).submit();
			}
			break;

		default:

			break;
		}
		break;

	default:

		break;
	}
}


void Main::_handle_fs_query_listing(Xml_node const &node)
{
	switch (_state) {
	case State::INVALID:
	{
		State const state { _state_from_fs_query_listing(node) };
		switch (state) {
		case State::INVALID:

			_state = State::SETUP_OBTAIN_PARAMETERS;
			Signal_transmitter(_state_handler).submit();
			break;

		case State::STARTUP_OBTAIN_PARAMETERS:

			_state = State::STARTUP_OBTAIN_PARAMETERS;
			Signal_transmitter(_state_handler).submit();
			break;

		default:

			class Unexpected_state { };
			throw Unexpected_state { };
		}
		break;
	}
	case State::CONTROLS_ROOT:
	case State::CONTROLS_SNAPSHOTS:
	case State::CONTROLS_DIMENSIONS:
	case State::CONTROLS_SECURITY:
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	case State::CONTROLS_SECURITY_MASTER_KEY:
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:
	{
		bool update_dialog { false };
		node.with_sub_node("dir", [&] (Xml_node const &node_0) {

			_snapshots.for_each([&] (Snapshot const &snap)
			{
				bool snap_still_exists { false };
				node_0.for_each_sub_node("dir", [&] (Xml_node const &node_1) {

					if (snap_still_exists) {
						return;
					}
					Generation const generation {
						node_1.attribute_value(
							"name", Generation { INVALID_GENERATION }) };

					if (generation == INVALID_GENERATION) {
						warning("skipping snapshot file with invalid generation number");
						return;
					}
					if (generation == snap.generation()) {
						snap_still_exists = true;
						return;
					}
				});
				if (!snap_still_exists) {

					if (_snapshots_select.valid() &&
					    &_snapshots_select.object() == &snap) {

						_snapshots_select = Snapshot_pointer { };
					}
					if (_snapshots_hover.valid() &&
					    &_snapshots_hover.object() == &snap) {

						_snapshots_hover = Snapshot_pointer { };
					}
					destroy(&_heap, &const_cast<Snapshot&>(snap));
					update_dialog = true;
				}
			});

			node_0.for_each_sub_node("dir", [&] (Xml_node const &node_1) {

				Generation const generation {
					node_1.attribute_value(
						"name", Generation { INVALID_GENERATION }) };

				if (generation == INVALID_GENERATION) {
					warning("skipping snapshot file with invalid generation number");
					return;
				}
				bool snap_already_exists { false };
				_snapshots.for_each([&] (Snapshot const &snap)
				{
					if (generation == snap.generation()) {
						snap_already_exists = true;
					}
				});
				if (!snap_already_exists) {
					new (_heap) Registered<Snapshot>(_snapshots, generation);
					update_dialog = true;
				}
			});
		});
		if (update_dialog) {
			_dialog.trigger_update();
		}

		break;
	}
	default:

		break;
	}
}


void Main::_handle_state()
{
	_update_sandbox_config();
	_dialog.trigger_update();
}


Main::Main(Env &env)
:
	Xml_producer { "dialog" },
	_env         { env }
{
	_config.sigh(_config_handler);
	_handle_config();
	_update_sandbox_config();
}


bool Cbe_manager::Main::_child_succeeded(Xml_node    const &sandbox_state,
                                        Child_state const &child_state)
{
	Child_exit_state const exit_state { sandbox_state, child_state.start_name() };

	if (!exit_state.exists()) {
		class Child_doesnt_exist { };
		throw Child_doesnt_exist { };
	}
	if (exit_state.exited()) {

		if (exit_state.code() != 0) {
			class Child_exited_with_error { };
			throw Child_exited_with_error { };
		}
		return true;
	}
	return false;
}


void Cbe_manager::Main::handle_sandbox_state()
{
	Buffered_xml sandbox_state {
		_heap, "sandbox_state",
		[&] (Xml_generator &xml) {
			_sandbox.generate_state_report(xml);
		}
	};
	bool update_sandbox { false };
	bool update_dialog { false };
	sandbox_state.with_xml_node([&] (Xml_node const &sandbox_state) {

		switch (_state) {
		case State::SETUP_RUN_CBE_INIT_TRUST_ANCHOR:

			if (_child_succeeded(sandbox_state, _cbe_init_trust_anchor)) {

				_state = State::SETUP_RUN_CBE_INIT;
				update_dialog = true;
				update_sandbox = true;
			}
			break;

		case State::STARTUP_RUN_CBE_INIT_TRUST_ANCHOR:

			_if_child_exited(sandbox_state, _cbe_init_trust_anchor, [&] (int exit_code) {

				if (exit_code == 0) {

					_state = State::STARTUP_START_CBE_VFS;
					update_dialog = true;
					update_sandbox = true;

				} else {

					_state = State::STARTUP_OBTAIN_PARAMETERS;
					_startup_failed = true;
					_setup_obtain_params_passphrase_1 = Input_passphrase { };
					_setup_obtain_params_select = Setup_obtain_params_select::PASSPHRASE_1_INPUT;
					update_dialog = true;
					update_sandbox = true;
				}
			});
			break;

		case State::SETUP_RUN_CBE_INIT:

			if (_child_succeeded(sandbox_state, _cbe_init)) {

				_state = State::SETUP_START_CBE_VFS;
				update_dialog = true;
				update_sandbox = true;
			}
			break;

		case State::SETUP_START_CBE_VFS:

			if (_child_succeeded(sandbox_state, _sync_to_cbe_vfs_init)) {

				_state = State::SETUP_FORMAT_CBE;
				update_dialog = true;
				update_sandbox = true;
			}
			break;

		case State::STARTUP_START_CBE_VFS:

			if (_child_succeeded(sandbox_state, _sync_to_cbe_vfs_init)) {

				_state = State::CONTROLS_ROOT;
				update_dialog = true;
				update_sandbox = true;
			}
			break;

		case State::SETUP_FORMAT_CBE:

			if (_child_succeeded(sandbox_state, _mke2fs)) {

				_write_to_state_file(State::STARTUP_OBTAIN_PARAMETERS);
				_state = State::CONTROLS_ROOT;
				update_dialog = true;
				update_sandbox = true;
			}
			break;

		case State::CONTROLS_ROOT:
		case State::CONTROLS_SNAPSHOTS:
		case State::CONTROLS_DIMENSIONS:
		case State::CONTROLS_SECURITY:
		case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
		case State::CONTROLS_SECURITY_MASTER_KEY:
		case State::CONTROLS_SECURITY_USER_PASSPHRASE:

			switch (_resizing_state) {
			case Resizing_state::ISSUE_REQUEST_AT_DEVICE:

				if (_child_succeeded(sandbox_state, _resizing_fs_tool)) {

					_resizing_state = Resizing_state::IN_PROGRESS_AT_DEVICE;
					update_dialog = true;
					update_sandbox = true;
				}
				break;

			default:

				break;
			}

			switch (_rekeying_state) {
			case Rekeying_state::ISSUE_REQUEST_AT_DEVICE:

				if (_child_succeeded(sandbox_state, _rekeying_fs_tool)) {

					_rekeying_state = Rekeying_state::IN_PROGRESS_AT_DEVICE;
					update_dialog = true;
					update_sandbox = true;
				}
				break;

			default:

				break;
			}

			switch (_create_snap_state) {
			case Create_snapshot_state::ISSUE_REQUEST_AT_DEVICE:

				if (_child_succeeded(sandbox_state, _create_snap_fs_tool)) {

					_create_snap_state = Create_snapshot_state::INACTIVE;
					update_dialog = true;
					update_sandbox = true;
				}
				break;

			default:

				break;
			}

			switch (_discard_snap_state) {
			case Discard_snapshot_state::ISSUE_REQUEST_AT_DEVICE:

				if (_child_succeeded(sandbox_state, _discard_snap_fs_tool)) {

					_discard_snap_state = Discard_snapshot_state::INACTIVE;
					update_dialog = true;
					update_sandbox = true;
				}
				break;

			default:

				break;
			}

			break;

		case State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE:

			if (_child_succeeded(sandbox_state, _shut_down_fs_tool)) {

				_state = State::SHUTDOWN_WAIT_TILL_DEINIT_REQUEST_IS_DONE;
				update_dialog = true;
				update_sandbox = true;
			}
			break;

		default:

			break;
		}
		sandbox_state.for_each_sub_node("child", [&] (Xml_node const &child_node) {
			_children.for_each([&] (Child_state &child_state) {
				if (child_state.apply_child_state_report(child_node)) {
					update_sandbox = true;
				}
			});
		});
	});
	if (update_dialog) {
		_dialog.trigger_update();
	}
	if (update_sandbox) {
		_update_sandbox_config();
	}
}


void Cbe_manager::Main::produce_xml(Xml_generator &xml)
{
	switch (_state) {
	case State::INVALID:

		gen_titled_info_frame(xml, "1", "Program initialization", "Reading state file", MAIN_FRAME_WIDTH);
		break;

	case State::SETUP_OBTAIN_PARAMETERS:

		gen_titled_frame(xml, "1", _setup_title, MAIN_FRAME_WIDTH, [&] (Xml_generator &xml) {

			bool gen_start_button { true };
			gen_titled_text_input(
				xml, "pw1", "Enter passphrase twice",
				_setup_obtain_params_passphrase_1,
				_setup_obtain_params_select == Setup_obtain_params_select::PASSPHRASE_1_INPUT);

			gen_text_input(
				xml, "pw2",
				_setup_obtain_params_passphrase_2,
				_setup_obtain_params_select == Setup_obtain_params_select::PASSPHRASE_2_INPUT);

			if (!_setup_obtain_params_passphrase_1.suitable()) {

				gen_start_button = false;
				gen_info_line(xml, "info_1", "Passphrase too short!");

			} else if (!_setup_obtain_params_passphrase_2.equals(_setup_obtain_params_passphrase_1)) {

				gen_start_button = false;
				gen_info_line(xml, "info_1", "Passphrases differ!");
			}
			gen_info_line(xml, "pad_1", "");
			gen_titled_text_input(
				xml, "sz", "Size in bytes (suffixes K, M, G)",
				_setup_obtain_params_size,
				_setup_obtain_params_select == Setup_obtain_params_select::SIZE_INPUT);

			if (!_setup_obtain_params_size.is_nr_of_bytes_greater_than_zero()) {

				gen_start_button = false;
				gen_info_line(xml, "info_2", "Must be a number greater than 0");

			} else {

				gen_info_line(
					xml, "info_2",
					String<256> { "Image size will be ", _cbe_size()}.string());
			}
			gen_info_line(xml, "pad_2", "");
			if (gen_start_button) {

				gen_action_button_at_bottom(
					xml, "ok", "Start",
					_setup_obtain_params_hover == Setup_obtain_params_hover::START_BUTTON,
					_setup_obtain_params_select == Setup_obtain_params_select::START_BUTTON);
			}
		});
		break;

	case State::STARTUP_OBTAIN_PARAMETERS:

		gen_titled_frame(xml, "1", _startup_title, MAIN_FRAME_WIDTH, [&] (Xml_generator &xml) {

			if (_startup_failed) {

				gen_info_line(xml, "info_1", "Startup failed! Please try again.");
				gen_info_line(xml, "pad_1", "");
			}
			bool gen_start_button { true };
			gen_titled_text_input(
				xml, "pw1", "Trust anchor passphrase",
				_setup_obtain_params_passphrase_1,
				_setup_obtain_params_select == Setup_obtain_params_select::PASSPHRASE_1_INPUT);

			if (!_setup_obtain_params_passphrase_1.suitable()) {

				gen_start_button = false;
				gen_info_line(xml, "info_2", _setup_obtain_params_passphrase_1.not_suitable_text());
			}
			gen_info_line(xml, "pad_2", "");
			if (gen_start_button) {

				gen_action_button_at_bottom(
					xml, "ok", "Start",
					_setup_obtain_params_hover == Setup_obtain_params_hover::START_BUTTON,
					_setup_obtain_params_select == Setup_obtain_params_select::START_BUTTON);
			}
		});
		break;

	case State::SETUP_RUN_CBE_INIT_TRUST_ANCHOR:

		gen_titled_info_frame(xml, "1", _setup_title, "Initializing trust anchor", MAIN_FRAME_WIDTH);
		break;

	case State::SETUP_CREATE_CBE_IMAGE_FILE:

		gen_titled_info_frame(xml, "1", _setup_title, "Creating image file", MAIN_FRAME_WIDTH);
		break;

	case State::SETUP_RUN_CBE_INIT:

		gen_titled_info_frame(xml, "1", _setup_title, "Initializing device", MAIN_FRAME_WIDTH);
		break;

	case State::SETUP_START_CBE_VFS:

		gen_titled_info_frame(xml, "1", _setup_title, "Starting device driver", MAIN_FRAME_WIDTH);
		break;

	case State::SETUP_FORMAT_CBE:

		gen_titled_info_frame(xml, "1", _setup_title, "Initializing Ext2 FS", MAIN_FRAME_WIDTH);
		break;

	case State::STARTUP_RUN_CBE_INIT_TRUST_ANCHOR:

		gen_titled_info_frame(xml, "1", _startup_title, "Unlocking trust anchor", MAIN_FRAME_WIDTH);
		break;

	case State::STARTUP_START_CBE_VFS:

		gen_titled_info_frame(xml, "1", _startup_title, "Starting device driver", MAIN_FRAME_WIDTH);
		break;

	case State::CONTROLS_ROOT:

		gen_titled_frame(xml, "app", _controls_title, MAIN_FRAME_WIDTH, [&] (Xml_generator &xml) {

			xml.node("hbox", [&] () {

				gen_action_button(xml, "Shut down", "Shut down",
					_controls_root_hover  == Controls_root_hover::SHUT_DOWN_BUTTON,
					_controls_root_select == Controls_root_select::SHUT_DOWN_BUTTON);
			});
			xml.node("frame", [&] () {

				xml.node("vbox", [&] () {

					gen_closed_sub_menu(
						xml, "Snapshots",
						_controls_root_hover == Controls_root_hover::SNAPSHOTS_EXPAND_BUTTON);

					gen_closed_sub_menu(
						xml, "Dimensions",
						_controls_root_hover == Controls_root_hover::DIMENSIONS_EXPAND_BUTTON);

					gen_closed_sub_menu(
						xml, "Security",
						_controls_root_hover == Controls_root_hover::SECURITY_EXPAND_BUTTON);
				});
			});
		});
		break;

	case State::CONTROLS_SNAPSHOTS:

		gen_titled_frame(xml, "app", _controls_title, MAIN_FRAME_WIDTH, [&] (Xml_generator &xml) {

			xml.node("hbox", [&] () {

				gen_action_button(xml, "Shut down", "Shut down",
					_controls_snapshots_hover  == Controls_snapshots_hover::SHUT_DOWN_BUTTON,
					_controls_snapshots_select == Controls_snapshots_select::SHUT_DOWN_BUTTON);
			});
			xml.node("frame", [&] () {

				xml.node("vbox", [&] () {

					gen_opened_sub_menu(
						xml, "Snapshots",
						_controls_snapshots_hover == Controls_snapshots_hover::SNAPSHOTS_EXPAND_BUTTON,
						[&] (Xml_generator &xml)
					{
						_snapshots.for_each([&] (Snapshot const &snap) {

							bool const hovered {
								_snapshots_hover.valid() &&
								_snapshots_hover.object().generation() == snap.generation() };

							bool const selected {
								_snapshots_select.valid() &&
								_snapshots_select.object().generation() == snap.generation() };

							String<64> const snap_str {
								"Generation ", snap.generation() };

							Generation_string const gen_str { snap.generation() };

							gen_multiple_choice_entry(
								xml, gen_str.string(), snap_str.string(), hovered,
								selected);

							if (selected) {

								bool const discard_hovered { _controls_snapshots_hover  == Controls_snapshots_hover::DISCARD_SNAPSHOT_BUTTON };
								bool const discard_selected { _controls_snapshots_select == Controls_snapshots_select::DISCARD_SNAPSHOT_BUTTON };

								switch(_discard_snap_state) {
								case Discard_snapshot_state::INACTIVE:

									xml.node("float", [&] () {
										xml.attribute("name", String<32> { "discard", gen_str });
										xml.attribute("west", "yes");

										xml.node("hbox", [&] () {

											xml.node("label", [&] () {
												xml.attribute("min_ex", "4");
											});
											xml.node("button", [&] () {
												if (discard_hovered) {
													xml.attribute("hovered", "yes");
												}
												if (discard_selected) {
													xml.attribute("selected", "yes");
												}

												xml.node("label", [&] () {
													xml.attribute("text", "Discard");
												});
											});
										});
									});
									break;

								case Discard_snapshot_state::ISSUE_REQUEST_AT_DEVICE:

									xml.node("float", [&] () {
										xml.attribute("name", String<32> { "inactive_discard", gen_str });
										xml.attribute("west", "yes");

										xml.node("hbox", [&] () {

											xml.node("label", [&] () {
												xml.attribute("min_ex", "4");
											});
											xml.node("button", [&] () {
												xml.attribute("name", gen_str.string());
												if (discard_hovered) {
													xml.attribute("hovered", "yes");
												}
												if (discard_selected) {
													xml.attribute("selected", "yes");
												}
												xml.node("hbox", [&] () {

													xml.node("label", [&] () {
														xml.attribute("text", "...");
													});
												});
											});
										});
									});
									break;
								}
							}
						});

						bool const hovered { _controls_snapshots_hover  == Controls_snapshots_hover::CREATE_SNAPSHOT_BUTTON };
						bool const selected { _controls_snapshots_select == Controls_snapshots_select::CREATE_SNAPSHOT_BUTTON };

						switch(_create_snap_state) {
						case Create_snapshot_state::INACTIVE:

							xml.node("float", [&] () {
								xml.attribute("name", "create");
								xml.attribute("west", "yes");

								xml.node("hbox", [&] () {

									xml.node("button", [&] () {
										if (hovered) {
											xml.attribute("hovered", "yes");
										}
										if (selected) {
											xml.attribute("selected", "yes");
										}
										xml.node("hbox", [&] () {

											xml.node("label", [&] () {
												xml.attribute("text", "Create");
											});
										});
									});
								});
							});
							break;

						case Create_snapshot_state::ISSUE_REQUEST_AT_DEVICE:

							xml.node("float", [&] () {
								xml.attribute("name", "inactive_create");
								xml.attribute("west", "yes");

								xml.node("hbox", [&] () {

									xml.node("button", [&] () {
										if (hovered) {
											xml.attribute("hovered", "yes");
										}
										if (selected) {
											xml.attribute("selected", "yes");
										}
										xml.node("hbox", [&] () {

											xml.node("label", [&] () {
												xml.attribute("text", "...");
											});
										});
									});
								});
							});
							break;
						}
					});
				});
			});
		});
		break;

	case State::CONTROLS_DIMENSIONS:

		gen_titled_frame(xml, "app", _controls_title, MAIN_FRAME_WIDTH, [&] (Xml_generator &xml) {

			xml.node("hbox", [&] () {

				gen_action_button(xml, "Shut down", "Shut down",
					_controls_dimensions_hover  == Controls_dimensions_hover::SHUT_DOWN_BUTTON,
					_controls_dimensions_select == Controls_dimensions_select::SHUT_DOWN_BUTTON);
			});
			xml.node("frame", [&] () {

				xml.node("vbox", [&] () {

					gen_opened_sub_menu(
						xml, "Dimensions",
						_controls_dimensions_hover == Controls_dimensions_hover::DIMENSIONS_EXPAND_BUTTON,
						[&] (Xml_generator &xml)
					{
						switch(_resizing_state) {
						case Resizing_state::INACTIVE:
						{
							gen_titled_text_input(
								xml, "blks", "Number of blocks",
								_resizing_nr_of_blks,
								_controls_dimensions_select == Controls_dimensions_select::RESIZING_NR_OF_BLKS_INPUT);

							bool gen_start_button { true };
							if (!_resizing_nr_of_blks.is_nr_greater_than_zero()) {

								gen_start_button = false;
								gen_info_line(xml, "inf", "Must be a number greater than 0");
								gen_info_line(xml, "pad_1", "");

							}  else {

								Number_of_bytes const curr_cbe_size { _cbe_size() };
								unsigned long rsz_nr_of_bytes {
									_resizing_nr_of_blks.to_unsigned_long() *
									CBE_BLOCK_SIZE };

								gen_info_line(
									xml, "inf_1",
									String<256> {
										"Current image size: ",
										curr_cbe_size
									}.string());

								gen_info_line(
									xml, "inf_2",
									String<256> {
										"New image size: ",
										Number_of_bytes { curr_cbe_size + rsz_nr_of_bytes }
									}.string());

								gen_info_line(xml, "pad_1", "");
							}
							if (gen_start_button) {

								gen_action_button_at_bottom(
									xml, "Start",
									_controls_dimensions_hover  == Controls_dimensions_hover::RESIZING_START_BUTTON,
									_controls_dimensions_select == Controls_dimensions_select::RESIZING_START_BUTTON);
							}
							break;
						}
						case Resizing_state::WAIT_TILL_DEVICE_IS_READY:

							gen_info_line(xml, "inf", "Wait for device...");
							gen_info_line(xml, "pad_1", "");
							break;

						case Resizing_state::ISSUE_REQUEST_AT_DEVICE:

							gen_info_line(xml, "inf", "Initiate...");
							gen_info_line(xml, "pad_1", "");
							break;

						case Resizing_state::IN_PROGRESS_AT_DEVICE:

							gen_info_line(xml, "inf", "In progress...");
							gen_info_line(xml, "pad_1", "");
							break;
						}
					});
				});
			});
		});
		break;

	case State::CONTROLS_SECURITY:

		gen_titled_frame(xml, "app", _controls_title, MAIN_FRAME_WIDTH, [&] (Xml_generator &xml) {

			xml.node("hbox", [&] () {

				gen_action_button(xml, "Shut down", "Shut down",
					_controls_security_hover  == Controls_security_hover::SHUT_DOWN_BUTTON,
					_controls_security_select == Controls_security_select::SHUT_DOWN_BUTTON);
			});
			xml.node("frame", [&] () {

				gen_opened_sub_menu(
					xml, "Security",
					_controls_security_hover == Controls_security_hover::SECURITY_EXPAND_BUTTON,
					[&] (Xml_generator &xml)
				{
					gen_closed_sub_menu(
						xml, "Block Encryption Key",
						_controls_security_hover == Controls_security_hover::BLOCK_ENCRYPTION_KEY_EXPAND_BUTTON);

					gen_closed_sub_menu(
						xml, "Master Key",
						_controls_security_hover == Controls_security_hover::MASTER_KEY_EXPAND_BUTTON);

					gen_closed_sub_menu(
						xml, "User Passphrase",
						_controls_security_hover == Controls_security_hover::USER_PASSPHRASE_EXPAND_BUTTON);
				});
			});
		});
		break;

	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:

		gen_titled_frame(xml, "app", _controls_title, MAIN_FRAME_WIDTH, [&] (Xml_generator &xml) {

			xml.node("hbox", [&] () {

				gen_action_button(xml, "Shut down", "Shut down",
					_controls_security_block_encryption_key_hover  == Controls_security_block_encryption_key_hover::SHUT_DOWN_BUTTON,
					_controls_security_block_encryption_key_select == Controls_security_block_encryption_key_select::SHUT_DOWN_BUTTON);
			});
			xml.node("frame", [&] () {

				gen_opened_sub_menu(
					xml, "Block Encryption Key",
					_controls_security_block_encryption_key_hover == Controls_security_block_encryption_key_hover::LEAVE_BUTTON,
					[&] (Xml_generator &xml)
				{
					switch(_rekeying_state) {
					case Rekeying_state::INACTIVE:

						gen_action_button(xml, "Rekey", "Replace",
							_controls_security_block_encryption_key_hover  == Controls_security_block_encryption_key_hover::REPLACE_BUTTON,
							_controls_security_block_encryption_key_select == Controls_security_block_encryption_key_select::REPLACE_BUTTON);

						break;

					case Rekeying_state::WAIT_TILL_DEVICE_IS_READY:
					case Rekeying_state::ISSUE_REQUEST_AT_DEVICE:
					case Rekeying_state::IN_PROGRESS_AT_DEVICE:

						gen_action_button(xml, "Inactive Rekey", "...",
							_controls_security_block_encryption_key_hover == Controls_security_block_encryption_key_hover::REPLACE_BUTTON,
							false);

						break;
					}
					gen_info_line(xml, "pad_1", "");
				});
			});
		});
		break;

	case State::CONTROLS_SECURITY_MASTER_KEY:

		gen_titled_frame(xml, "app", _controls_title, MAIN_FRAME_WIDTH, [&] (Xml_generator &xml) {

			xml.node("hbox", [&] () {

				gen_action_button(xml, "Shut down", "Shut down",
					_controls_security_master_key_hover  == Controls_security_master_key_hover::SHUT_DOWN_BUTTON,
					_controls_security_master_key_select == Controls_security_master_key_select::SHUT_DOWN_BUTTON);
			});
			xml.node("frame", [&] () {

				gen_opened_sub_menu(
					xml, "Master Key",
					_controls_security_master_key_hover == Controls_security_master_key_hover::LEAVE_BUTTON,
					[&] (Xml_generator &xml)
				{
					gen_info_line(xml, "pad_1", "");
					gen_info_line(xml, "inf_1", "The master key cannot be replaced by now.");
					gen_info_line(xml, "pad_2", "");
				});
			});
		});
		break;

	case State::CONTROLS_SECURITY_USER_PASSPHRASE:

		gen_titled_frame(xml, "app", _controls_title, MAIN_FRAME_WIDTH, [&] (Xml_generator &xml) {

			xml.node("hbox", [&] () {

				gen_action_button(xml, "Shut down", "Shut down",
					_controls_security_user_passphrase_hover  == Controls_security_user_passphrase_hover::SHUT_DOWN_BUTTON,
					_controls_security_user_passphrase_select == Controls_security_user_passphrase_select::SHUT_DOWN_BUTTON);
			});
			xml.node("frame", [&] () {

				gen_opened_sub_menu(
					xml, "User Passphrase",
					_controls_security_user_passphrase_hover == Controls_security_user_passphrase_hover::LEAVE_BUTTON,
					[&] (Xml_generator &xml)
				{
					gen_info_line(xml, "pad_1", "");
					gen_info_line(xml, "inf_1", "The user passphrase cannot be replaced by now.");
					gen_info_line(xml, "pad_2", "");
				});
			});
		});
		break;

	case State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE:

		gen_titled_info_frame(xml, "1", _shutdown_title, "Send request to device", MAIN_FRAME_WIDTH);
		break;

	case State::SHUTDOWN_WAIT_TILL_DEINIT_REQUEST_IS_DONE:

		gen_titled_info_frame(xml, "1", _shutdown_title, "Wait for device", MAIN_FRAME_WIDTH);
		break;
	}
}


void Cbe_manager::Main::wakeup_local_service()
{
	_rom_service.for_each_requested_session([&] (Rom_service::Request &request) {

		if (request.label == "menu_view -> dialog")
			request.deliver_session(_dialog);
		else
			request.deny();
	});

	_report_service.for_each_requested_session([&] (Report_service::Request &request) {

		if (request.label == "fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _fs_query_listing_handler, _env.ep(),
					request.resources, "", request.diag) };

			request.deliver_session(session);

		} else if (request.label == "resizing_fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _resizing_fs_query_listing_handler, _env.ep(),
					request.resources, "", request.diag) };

			request.deliver_session(session);

		} else if (request.label == "rekeying_fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _rekeying_fs_query_listing_handler, _env.ep(),
					request.resources, "", request.diag) };

			request.deliver_session(session);

		} else if (request.label == "shut_down_fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _shut_down_fs_query_listing_handler, _env.ep(),
					request.resources, "", request.diag) };

			request.deliver_session(session);
		}
	});

	_report_service.for_each_requested_session([&] (Report_service::Request &request) {

		if (request.label == "menu_view -> hover") {
			Report::Session_component &session = *new (_heap)
				Report::Session_component(_env, _hover_handler,
				                          _env.ep(),
				                          request.resources, "", request.diag);
			request.deliver_session(session);
		}
	});

	_report_service.for_each_session_to_close([&] (Report::Session_component &session) {

		destroy(_heap, &session);
		return Report_service::Close_response::CLOSED;
	});

	_gui_service.for_each_requested_session([&] (Gui_service::Request &request) {

		Gui::Session_component &session = *new (_heap)
			Gui::Session_component(_env, *this, _env.ep(),
			                       request.resources, "", request.diag);

		request.deliver_session(session);
	});

	_gui_service.for_each_upgraded_session([&] (Gui::Session_component &session,
	                                            Session::Resources const &amount) {
		session.upgrade(amount);
		return Gui_service::Upgrade_response::CONFIRMED;
	});

	_gui_service.for_each_session_to_close([&] (Gui::Session_component &session) {

		destroy(_heap, &session);
		return Gui_service::Close_response::CLOSED;
	});
}


size_t Main::_init_cbe_nr_of_leafs() const
{
	size_t const size { _setup_obtain_params_size.to_nr_of_bytes() };
	size_t nr_of_leafs { size / CBE_BLOCK_SIZE };
	if (size % CBE_BLOCK_SIZE) {
		nr_of_leafs++;
	}
	return nr_of_leafs;
}


void Cbe_manager::Main::_generate_sandbox_config(Xml_generator &xml) const
{
	switch (_state) {
	case State::INVALID:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		gen_fs_query_start_node(xml, _fs_query);
		break;

	case State::SETUP_OBTAIN_PARAMETERS:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		break;

	case State::STARTUP_OBTAIN_PARAMETERS:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		break;

	case State::SETUP_RUN_CBE_INIT_TRUST_ANCHOR:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_init_trust_anchor_start_node(
			xml, _cbe_init_trust_anchor, _setup_obtain_params_passphrase_1);

		break;

	case State::STARTUP_RUN_CBE_INIT_TRUST_ANCHOR:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_init_trust_anchor_start_node(
			xml, _cbe_init_trust_anchor, _setup_obtain_params_passphrase_1);

		break;

	case State::STARTUP_START_CBE_VFS:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_vfs_start_node(xml, _cbe_vfs);
		gen_sync_to_cbe_vfs_init_start_node(xml, _sync_to_cbe_vfs_init);
		break;

	case State::SETUP_CREATE_CBE_IMAGE_FILE:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		break;

	case State::SETUP_RUN_CBE_INIT:
	{
		Tree_geometry const tree_geom {
			INIT_CBE_NR_OF_LEVELS,
			INIT_CBE_NR_OF_CHILDREN,
			_init_cbe_nr_of_leafs() };

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_image_vfs_block_start_node(xml, _cbe_image_vfs_block);
		gen_cbe_init_start_node(xml, _cbe_init, tree_geom, tree_geom);
		break;
	}
	case State::SETUP_START_CBE_VFS:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_vfs_start_node(xml, _cbe_vfs);
		gen_sync_to_cbe_vfs_init_start_node(xml, _sync_to_cbe_vfs_init);
		break;

	case State::SETUP_FORMAT_CBE:

		gen_parent_provides_and_report_nodes(xml);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_vfs_start_node(xml, _cbe_vfs);
		gen_cbe_vfs_block_start_node(xml, _cbe_vfs_block);
		gen_mke2fs_start_node(xml, _mke2fs);
		break;

	case State::CONTROLS_ROOT:
	case State::CONTROLS_SNAPSHOTS:
	case State::CONTROLS_DIMENSIONS:
	case State::CONTROLS_SECURITY:
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	case State::CONTROLS_SECURITY_MASTER_KEY:
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:

		gen_parent_provides_and_report_nodes(xml);
		gen_policy_for_child_service(xml, "File_system", _rump_vfs);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_vfs_start_node(xml, _cbe_vfs);
		gen_cbe_vfs_block_start_node(xml, _cbe_vfs_block);
		gen_rump_vfs_start_node(xml, _rump_vfs);
		gen_snapshots_fs_query_start_node(xml, _snapshots_fs_query);

		switch(_resizing_state) {
		case Resizing_state::INACTIVE:

			break;

		case Resizing_state::WAIT_TILL_DEVICE_IS_READY:

			gen_resizing_fs_query_start_node(xml, _resizing_fs_query);
			break;

		case Resizing_state::ISSUE_REQUEST_AT_DEVICE:

			gen_resizing_fs_tool_start_node(
				xml, _resizing_fs_tool,
				_resizing_nr_of_blks.to_unsigned_long());

			break;

		case Resizing_state::IN_PROGRESS_AT_DEVICE:

			gen_resizing_fs_query_start_node(xml, _resizing_fs_query);
			break;
		}

		switch(_rekeying_state) {
		case Rekeying_state::INACTIVE:

			break;

		case Rekeying_state::WAIT_TILL_DEVICE_IS_READY:

			gen_rekeying_fs_query_start_node(xml, _rekeying_fs_query);
			break;

		case Rekeying_state::ISSUE_REQUEST_AT_DEVICE:

			gen_rekeying_fs_tool_start_node(xml, _rekeying_fs_tool);
			break;

		case Rekeying_state::IN_PROGRESS_AT_DEVICE:

			gen_rekeying_fs_query_start_node(xml, _rekeying_fs_query);
			break;
		}

		switch(_create_snap_state) {
		case Create_snapshot_state::INACTIVE:

			break;

		case Create_snapshot_state::ISSUE_REQUEST_AT_DEVICE:

			gen_create_snap_fs_tool_start_node(xml, _create_snap_fs_tool);
			break;
		}

		switch(_discard_snap_state) {
		case Discard_snapshot_state::INACTIVE:

			break;

		case Discard_snapshot_state::ISSUE_REQUEST_AT_DEVICE:

			gen_discard_snap_fs_tool_start_node(xml, _discard_snap_fs_tool, _discard_snap_gen);
			break;
		}

		break;

	case State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE:

		gen_parent_provides_and_report_nodes(xml);
		gen_policy_for_child_service(xml, "File_system", _rump_vfs);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_vfs_start_node(xml, _cbe_vfs);
		gen_cbe_vfs_block_start_node(xml, _cbe_vfs_block);
		gen_snapshots_fs_query_start_node(xml, _snapshots_fs_query);
		gen_shut_down_fs_tool_start_node(xml, _shut_down_fs_tool);
		break;

	case State::SHUTDOWN_WAIT_TILL_DEINIT_REQUEST_IS_DONE:

		gen_parent_provides_and_report_nodes(xml);
		gen_policy_for_child_service(xml, "File_system", _rump_vfs);
		gen_menu_view_start_node(xml, _menu_view);
		gen_cbe_trust_anchor_vfs_start_node(xml, _cbe_trust_anchor_vfs);
		gen_cbe_vfs_start_node(xml, _cbe_vfs);
		gen_cbe_vfs_block_start_node(xml, _cbe_vfs_block);
		gen_snapshots_fs_query_start_node(xml, _snapshots_fs_query);
		gen_shut_down_fs_query_start_node(xml, _shut_down_fs_query);
		break;
	}
}


size_t Main::_tree_nr_of_blocks(size_t nr_of_lvls,
                                size_t nr_of_children,
                                size_t nr_of_leafs)
{
	size_t nr_of_blks { 0 };
	size_t nr_of_last_lvl_blks { nr_of_leafs };
	for (size_t lvl_idx { 0 }; lvl_idx < nr_of_lvls; lvl_idx++) {
		nr_of_blks += nr_of_last_lvl_blks;
		if (nr_of_last_lvl_blks % nr_of_children) {
			nr_of_last_lvl_blks = nr_of_last_lvl_blks / nr_of_children + 1;
		} else {
			nr_of_last_lvl_blks = nr_of_last_lvl_blks / nr_of_children;
		}
	}
	return nr_of_blks;
}


Number_of_bytes Main::_cbe_size() const
{
	return
		Number_of_bytes {
			_cbe_nr_of_blocks(
				INIT_CBE_NR_OF_SUPERBLOCKS,
				INIT_CBE_NR_OF_LEVELS,
				INIT_CBE_NR_OF_CHILDREN,
				_init_cbe_nr_of_leafs(),
				INIT_CBE_NR_OF_LEVELS,
				INIT_CBE_NR_OF_CHILDREN,
				_init_cbe_nr_of_leafs())
			* CBE_BLOCK_SIZE };
}


size_t Main::_cbe_nr_of_blocks(size_t nr_of_superblocks,
                               size_t nr_of_vbd_lvls,
                               size_t nr_of_vbd_children,
                               size_t nr_of_vbd_leafs,
                               size_t nr_of_ft_lvls,
                               size_t nr_of_ft_children,
                               size_t nr_of_ft_leafs)
{
	size_t const nr_of_vbd_blks {
		_tree_nr_of_blocks(
			nr_of_vbd_lvls,
			nr_of_vbd_children,
			nr_of_vbd_leafs) };

	size_t const nr_of_ft_blks {
		_tree_nr_of_blocks(
			nr_of_ft_lvls,
			nr_of_ft_children,
			nr_of_ft_leafs) };

	/* FIXME
	 *
	 * This would be the correct way to calculate the number of MT blocks
	 * but the CBE still uses an MT the same size as the FT for simplicity
	 * reasons. As soon as the CBE does it right we should fix also this path.
	 *
	 *	size_t const nr_of_mt_leafs {
	 *		nr_of_ft_blks - nr_of_ft_leafs };
	 *
	 *	size_t const nr_of_mt_blks {
	 *		_tree_nr_of_blocks(
	 *			nr_of_mt_lvls,
	 *			nr_of_mt_children,
	 *			nr_of_mt_leafs) };
	 */
	size_t const nr_of_mt_blks { nr_of_ft_blks };

	return
		nr_of_superblocks +
		nr_of_vbd_blks +
		nr_of_ft_blks +
		nr_of_mt_blks;
}


void Cbe_manager::Main::handle_input_event(Input::Event const &event)
{
	bool update_dialog { false };
	bool update_sandbox_config { false };

	switch (_state) {
	case State::SETUP_OBTAIN_PARAMETERS:

		event.handle_press([&] (Input::Keycode key, Codepoint code) {

			if (key == Input::BTN_LEFT) {

				Setup_obtain_params_select const prev_select { _setup_obtain_params_select };
				Setup_obtain_params_select       next_select { Setup_obtain_params_select::NONE };

				switch (_setup_obtain_params_hover) {
				case Setup_obtain_params_hover::START_BUTTON:

					next_select = Setup_obtain_params_select::START_BUTTON;
					break;

				case Setup_obtain_params_hover::PASSPHRASE_1_INPUT:

					next_select = Setup_obtain_params_select::PASSPHRASE_1_INPUT;
					break;

				case Setup_obtain_params_hover::PASSPHRASE_2_INPUT:

					next_select = Setup_obtain_params_select::PASSPHRASE_2_INPUT;
					break;

				case Setup_obtain_params_hover::SIZE_INPUT:

					next_select = Setup_obtain_params_select::SIZE_INPUT;
					break;

				case Setup_obtain_params_hover::NONE:

					next_select = Setup_obtain_params_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_setup_obtain_params_select = next_select;
					update_dialog = true;
				}

			} else if (key == Input::KEY_ENTER) {

				if (_setup_obtain_params_size.is_nr_of_bytes_greater_than_zero() &&
				    _setup_obtain_params_passphrase_1.suitable() &&
				    _setup_obtain_params_passphrase_2.equals(_setup_obtain_params_passphrase_1) &&
				    _setup_obtain_params_select != Setup_obtain_params_select::START_BUTTON) {

					_setup_obtain_params_select = Setup_obtain_params_select::START_BUTTON;
					update_dialog = true;
				}

			} else if (key == Input::KEY_TAB) {

				if (_setup_obtain_params_select == Setup_obtain_params_select::PASSPHRASE_1_INPUT) {

					_setup_obtain_params_select = Setup_obtain_params_select::PASSPHRASE_2_INPUT;
					update_dialog = true;

				} else if (_setup_obtain_params_select == Setup_obtain_params_select::PASSPHRASE_2_INPUT) {

					_setup_obtain_params_select = Setup_obtain_params_select::SIZE_INPUT;
					update_dialog = true;

				} else if (_setup_obtain_params_select == Setup_obtain_params_select::SIZE_INPUT) {

					_setup_obtain_params_select = Setup_obtain_params_select::PASSPHRASE_1_INPUT;
					update_dialog = true;
				}

			} else {

				if (_setup_obtain_params_select == Setup_obtain_params_select::PASSPHRASE_1_INPUT) {

					if (codepoint_is_printable(code)) {

						_setup_obtain_params_passphrase_1.append_character(code);
						update_dialog = true;

					} else if (code.value == CODEPOINT_BACKSPACE) {

						_setup_obtain_params_passphrase_1.remove_last_character();
						update_dialog = true;
					}

				} else if (_setup_obtain_params_select == Setup_obtain_params_select::PASSPHRASE_2_INPUT) {

					if (codepoint_is_printable(code)) {

						_setup_obtain_params_passphrase_2.append_character(code);
						update_dialog = true;

					} else if (code.value == CODEPOINT_BACKSPACE) {

						_setup_obtain_params_passphrase_2.remove_last_character();
						update_dialog = true;
					}

				} else if (_setup_obtain_params_select == Setup_obtain_params_select::SIZE_INPUT) {

					if (codepoint_is_printable(code)) {

						_setup_obtain_params_size.append_character(code);
						update_dialog = true;

					} else if (code.value == CODEPOINT_BACKSPACE) {

						_setup_obtain_params_size.remove_last_character();
						update_dialog = true;

					}
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT ||
			    key == Input::KEY_ENTER) {

				if (_setup_obtain_params_size.is_nr_of_bytes_greater_than_zero() &&
				    _setup_obtain_params_passphrase_1.suitable() &&
				    _setup_obtain_params_passphrase_2.equals(_setup_obtain_params_passphrase_1) &&
				    _setup_obtain_params_select == Setup_obtain_params_select::START_BUTTON) {

					_setup_obtain_params_select = Setup_obtain_params_select::NONE;
					_state = State::SETUP_CREATE_CBE_IMAGE_FILE;

					_update_sandbox_config();
					_dialog.trigger_update();

					_vfs_create_zero_filled_file(
						_vfs, _heap, Directory::Path { "/cbe/cbe.img" },
						CBE_BLOCK_SIZE,
						_cbe_nr_of_blocks(
							INIT_CBE_NR_OF_SUPERBLOCKS,
							INIT_CBE_NR_OF_LEVELS,
							INIT_CBE_NR_OF_CHILDREN,
							_init_cbe_nr_of_leafs(),
							INIT_CBE_NR_OF_LEVELS,
							INIT_CBE_NR_OF_CHILDREN,
							_init_cbe_nr_of_leafs()));

					_state = State::SETUP_RUN_CBE_INIT_TRUST_ANCHOR;
					update_sandbox_config = true;
					update_dialog = true;
				}
			}
		});
		break;

	case State::STARTUP_OBTAIN_PARAMETERS:

		event.handle_press([&] (Input::Keycode key, Codepoint code) {

			if (key == Input::BTN_LEFT) {

				Setup_obtain_params_select const prev_select { _setup_obtain_params_select };
				Setup_obtain_params_select       next_select { Setup_obtain_params_select::NONE };

				switch (_setup_obtain_params_hover) {
				case Setup_obtain_params_hover::START_BUTTON:

					next_select = Setup_obtain_params_select::START_BUTTON;
					break;

				case Setup_obtain_params_hover::PASSPHRASE_1_INPUT:

					next_select = Setup_obtain_params_select::PASSPHRASE_1_INPUT;
					break;

				case Setup_obtain_params_hover::PASSPHRASE_2_INPUT:

					class Unexpected_hover_1 { };
					throw Unexpected_hover_1 { };

				case Setup_obtain_params_hover::SIZE_INPUT:

					class Unexpected_hover_2 { };
					throw Unexpected_hover_2 { };

				case Setup_obtain_params_hover::NONE:

					next_select = Setup_obtain_params_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_setup_obtain_params_select = next_select;
					update_dialog = true;
				}

			} else if (key == Input::KEY_ENTER) {

				if (_setup_obtain_params_passphrase_1.suitable() &&
				    _setup_obtain_params_select != Setup_obtain_params_select::START_BUTTON) {

					_setup_obtain_params_select = Setup_obtain_params_select::START_BUTTON;
					update_dialog = true;
				}

			} else {

				if (_setup_obtain_params_select == Setup_obtain_params_select::PASSPHRASE_1_INPUT) {

					if (codepoint_is_printable(code)) {

						_setup_obtain_params_passphrase_1.append_character(code);
						update_dialog = true;

					} else if (code.value == CODEPOINT_BACKSPACE) {

						_setup_obtain_params_passphrase_1.remove_last_character();
						update_dialog = true;
					}
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT ||
			    key == Input::KEY_ENTER) {

				if (_setup_obtain_params_passphrase_1.suitable() &&
				    _setup_obtain_params_select == Setup_obtain_params_select::START_BUTTON) {

					_setup_obtain_params_select = Setup_obtain_params_select::NONE;
					_state = State::STARTUP_RUN_CBE_INIT_TRUST_ANCHOR;
					update_sandbox_config = true;
					update_dialog = true;
				}
			}
		});
		break;

	case State::CONTROLS_ROOT:

		event.handle_press([&] (Input::Keycode key, Codepoint) {

			if (key == Input::BTN_LEFT) {

				Controls_root_select const prev_select { _controls_root_select };
				Controls_root_select       next_select { Controls_root_select::NONE };

				switch (_controls_root_hover) {
				case Controls_root_hover::SNAPSHOTS_EXPAND_BUTTON:

					_state = State::CONTROLS_SNAPSHOTS;
					update_dialog = true;
					break;

				case Controls_root_hover::DIMENSIONS_EXPAND_BUTTON:

					_state = State::CONTROLS_DIMENSIONS;
					update_dialog = true;
					break;

				case Controls_root_hover::SECURITY_EXPAND_BUTTON:

					_state = State::CONTROLS_SECURITY;
					update_dialog = true;
					break;

				case Controls_root_hover::SHUT_DOWN_BUTTON:

					next_select = Controls_root_select::SHUT_DOWN_BUTTON;
					break;

				case Controls_root_hover::NONE:

					next_select = Controls_root_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_controls_root_select = next_select;
					update_dialog = true;
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT) {

				switch (_controls_root_select) {
				case Controls_root_select::SHUT_DOWN_BUTTON:

					_controls_root_select = Controls_root_select::NONE;
					_state = State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				default:

					break;
				}
			}
		});
		break;

	case State::CONTROLS_SNAPSHOTS:

		event.handle_press([&] (Input::Keycode key, Codepoint) {

			if (key == Input::BTN_LEFT) {

				Controls_snapshots_select const prev_select { _controls_snapshots_select };
				Controls_snapshots_select       next_select { Controls_snapshots_select::NONE };

				switch (_controls_snapshots_hover) {
				case Controls_snapshots_hover::SNAPSHOTS_EXPAND_BUTTON:

					_state = State::CONTROLS_ROOT;
					update_dialog = true;
					break;

				case Controls_snapshots_hover::SHUT_DOWN_BUTTON:

					next_select = Controls_snapshots_select::SHUT_DOWN_BUTTON;
					break;

				case Controls_snapshots_hover::CREATE_SNAPSHOT_BUTTON:

					next_select = Controls_snapshots_select::CREATE_SNAPSHOT_BUTTON;
					break;

				case Controls_snapshots_hover::DISCARD_SNAPSHOT_BUTTON:

					next_select = Controls_snapshots_select::DISCARD_SNAPSHOT_BUTTON;
					break;

				case Controls_snapshots_hover::NONE:

					next_select = Controls_snapshots_select::NONE;
					break;
				}
				if (_snapshots_hover.valid()) {

					if (_snapshots_hover != _snapshots_select) {

						_snapshots_select = _snapshots_hover;
						update_dialog = true;
					} else {

						_snapshots_select = Snapshot_pointer { };
						update_dialog = true;
					}
				}
				if (next_select != prev_select) {

					_controls_snapshots_select = next_select;
					update_dialog = true;
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT) {

				switch (_controls_snapshots_select) {
				case Controls_snapshots_select::SHUT_DOWN_BUTTON:

					_controls_snapshots_select = Controls_snapshots_select::NONE;
					_state = State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				case Controls_snapshots_select::CREATE_SNAPSHOT_BUTTON:

					_controls_snapshots_select = Controls_snapshots_select::NONE;
					_create_snap_state = Create_snapshot_state::ISSUE_REQUEST_AT_DEVICE;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				case Controls_snapshots_select::DISCARD_SNAPSHOT_BUTTON:

					_controls_snapshots_select = Controls_snapshots_select::NONE;

					if (_snapshots_select.valid()) {
						_discard_snap_state = Discard_snapshot_state::ISSUE_REQUEST_AT_DEVICE;
						_discard_snap_gen = _snapshots_select.object().generation();
					}
					update_sandbox_config = true;
					update_dialog = true;
					break;

				default:

					break;
				}
			}
		});
		break;

	case State::CONTROLS_DIMENSIONS:

		event.handle_press([&] (Input::Keycode key, Codepoint code) {

			if (key == Input::BTN_LEFT) {

				Controls_dimensions_select const prev_select { _controls_dimensions_select };
				Controls_dimensions_select       next_select { Controls_dimensions_select::NONE };

				switch (_controls_dimensions_hover) {
				case Controls_dimensions_hover::DIMENSIONS_EXPAND_BUTTON:

					_state = State::CONTROLS_ROOT;
					update_dialog = true;
					break;

				case Controls_dimensions_hover::SHUT_DOWN_BUTTON:

					next_select = Controls_dimensions_select::SHUT_DOWN_BUTTON;
					break;

				case Controls_dimensions_hover::RESIZING_START_BUTTON:

					next_select = Controls_dimensions_select::RESIZING_START_BUTTON;
					break;

				case Controls_dimensions_hover::RESIZING_NR_OF_BLKS_INPUT:

					next_select = Controls_dimensions_select::RESIZING_NR_OF_BLKS_INPUT;
					break;

				case Controls_dimensions_hover::NONE:

					next_select = Controls_dimensions_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_controls_dimensions_select = next_select;
					update_dialog = true;
				}

			} else {

				if (_controls_dimensions_select == Controls_dimensions_select::RESIZING_NR_OF_BLKS_INPUT) {

					if (codepoint_is_printable(code)) {

						_resizing_nr_of_blks.append_character(code);
						update_dialog = true;

					} else if (code.value == CODEPOINT_BACKSPACE) {

						_resizing_nr_of_blks.remove_last_character();
						update_dialog = true;
					}
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT) {

				switch (_controls_dimensions_select) {
				case Controls_dimensions_select::RESIZING_START_BUTTON:

					_controls_dimensions_select = Controls_dimensions_select::NONE;
					_resizing_state = Resizing_state::WAIT_TILL_DEVICE_IS_READY;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				case Controls_dimensions_select::SHUT_DOWN_BUTTON:

					_controls_dimensions_select = Controls_dimensions_select::NONE;
					_state = State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				default:

					break;
				}
			}
		});
		break;

	case State::CONTROLS_SECURITY:

		event.handle_press([&] (Input::Keycode key, Codepoint) {

			if (key == Input::BTN_LEFT) {

				Controls_security_select const prev_select { _controls_security_select };
				Controls_security_select       next_select { Controls_security_select::NONE };

				switch (_controls_security_hover) {
				case Controls_security_hover::SECURITY_EXPAND_BUTTON:

					_state = State::CONTROLS_ROOT;
					update_dialog = true;
					break;

				case Controls_security_hover::BLOCK_ENCRYPTION_KEY_EXPAND_BUTTON:

					_state = State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY;
					update_dialog = true;
					break;

				case Controls_security_hover::MASTER_KEY_EXPAND_BUTTON:

					_state = State::CONTROLS_SECURITY_MASTER_KEY;
					update_dialog = true;
					break;

				case Controls_security_hover::USER_PASSPHRASE_EXPAND_BUTTON:

					_state = State::CONTROLS_SECURITY_USER_PASSPHRASE;
					update_dialog = true;
					break;

				case Controls_security_hover::SHUT_DOWN_BUTTON:

					next_select = Controls_security_select::SHUT_DOWN_BUTTON;
					break;

				case Controls_security_hover::NONE:

					next_select = Controls_security_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_controls_security_select = next_select;
					update_dialog = true;
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT) {

				switch (_controls_security_select) {
				case Controls_security_select::SHUT_DOWN_BUTTON:

					_controls_security_select = Controls_security_select::NONE;
					_state = State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				default:

					break;
				}
			}
		});
		break;

	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:

		event.handle_press([&] (Input::Keycode key, Codepoint) {

			if (key == Input::BTN_LEFT) {

				Controls_security_block_encryption_key_select const prev_select { _controls_security_block_encryption_key_select };
				Controls_security_block_encryption_key_select       next_select { Controls_security_block_encryption_key_select::NONE };

				switch (_controls_security_block_encryption_key_hover) {
				case Controls_security_block_encryption_key_hover::LEAVE_BUTTON:

					_state = State::CONTROLS_SECURITY;
					update_dialog = true;
					break;

				case Controls_security_block_encryption_key_hover::REPLACE_BUTTON:

					next_select = Controls_security_block_encryption_key_select::REPLACE_BUTTON;
					break;

				case Controls_security_block_encryption_key_hover::SHUT_DOWN_BUTTON:

					next_select = Controls_security_block_encryption_key_select::SHUT_DOWN_BUTTON;
					break;

				case Controls_security_block_encryption_key_hover::NONE:

					next_select = Controls_security_block_encryption_key_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_controls_security_block_encryption_key_select = next_select;
					update_dialog = true;
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT) {

				switch (_controls_security_block_encryption_key_select) {
				case Controls_security_block_encryption_key_select::REPLACE_BUTTON:

					_controls_security_block_encryption_key_select = Controls_security_block_encryption_key_select::NONE;
					_rekeying_state = Rekeying_state::WAIT_TILL_DEVICE_IS_READY;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				case Controls_security_block_encryption_key_select::SHUT_DOWN_BUTTON:

					_controls_security_block_encryption_key_select = Controls_security_block_encryption_key_select::NONE;
					_state = State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				default:

					break;
				}
			}
		});
		break;

	case State::CONTROLS_SECURITY_MASTER_KEY:

		event.handle_press([&] (Input::Keycode key, Codepoint) {

			if (key == Input::BTN_LEFT) {

				Controls_security_master_key_select const prev_select { _controls_security_master_key_select };
				Controls_security_master_key_select       next_select { Controls_security_master_key_select::NONE };

				switch (_controls_security_master_key_hover) {
				case Controls_security_master_key_hover::LEAVE_BUTTON:

					_state = State::CONTROLS_SECURITY;
					update_dialog = true;
					break;

				case Controls_security_master_key_hover::SHUT_DOWN_BUTTON:

					next_select = Controls_security_master_key_select::SHUT_DOWN_BUTTON;
					break;

				case Controls_security_master_key_hover::NONE:

					next_select = Controls_security_master_key_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_controls_security_master_key_select = next_select;
					update_dialog = true;
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT) {

				switch (_controls_security_master_key_select) {
				case Controls_security_master_key_select::SHUT_DOWN_BUTTON:

					_controls_security_master_key_select = Controls_security_master_key_select::NONE;
					_state = State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				default:

					break;
				}
			}
		});
		break;

	case State::CONTROLS_SECURITY_USER_PASSPHRASE:

		event.handle_press([&] (Input::Keycode key, Codepoint) {

			if (key == Input::BTN_LEFT) {

				Controls_security_user_passphrase_select const prev_select { _controls_security_user_passphrase_select };
				Controls_security_user_passphrase_select       next_select { Controls_security_user_passphrase_select::NONE };

				switch (_controls_security_user_passphrase_hover) {
				case Controls_security_user_passphrase_hover::LEAVE_BUTTON:

					_state = State::CONTROLS_SECURITY;
					update_dialog = true;
					break;

				case Controls_security_user_passphrase_hover::SHUT_DOWN_BUTTON:

					next_select = Controls_security_user_passphrase_select::SHUT_DOWN_BUTTON;
					break;

				case Controls_security_user_passphrase_hover::NONE:

					next_select = Controls_security_user_passphrase_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_controls_security_user_passphrase_select = next_select;
					update_dialog = true;
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT) {

				switch (_controls_security_user_passphrase_select) {
				case Controls_security_user_passphrase_select::SHUT_DOWN_BUTTON:

					_controls_security_user_passphrase_select = Controls_security_user_passphrase_select::NONE;
					_state = State::SHUTDOWN_ISSUE_DEINIT_REQUEST_AT_CBE;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				default:

					break;
				}
			}
		});
		break;

	default:

		break;
	}
	if (update_sandbox_config) {
		_update_sandbox_config();
	}
	if (update_dialog) {
		_dialog.trigger_update();
	}
}


void Cbe_manager::Main::_handle_hover(Xml_node const &node)
{
	bool update_dialog { false };

	switch (_state) {
	case State::SETUP_OBTAIN_PARAMETERS:
	{
		Setup_obtain_params_hover const prev_hover { _setup_obtain_params_hover };
		Setup_obtain_params_hover       next_hover { Setup_obtain_params_hover::NONE };

		node.with_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_sub_node("vbox", [&] (Xml_node const &node_2) {

					node_2.with_sub_node("float", [&] (Xml_node const &node_3) {
						if (node_3.attribute_value("name", String<3>()) == "ok") {
							next_hover = Setup_obtain_params_hover::START_BUTTON;
						}
					});

					node_2.with_sub_node("frame", [&] (Xml_node const &node_3) {

						if (node_3.attribute_value("name", String<4>()) == "pw1") {
							next_hover = Setup_obtain_params_hover::PASSPHRASE_1_INPUT;

						} else if (node_3.attribute_value("name", String<4>()) == "pw2") {
							next_hover = Setup_obtain_params_hover::PASSPHRASE_2_INPUT;

						} else if (node_3.attribute_value("name", String<4>()) == "sz") {
							next_hover = Setup_obtain_params_hover::SIZE_INPUT;
						}
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_setup_obtain_params_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::STARTUP_OBTAIN_PARAMETERS:
	{
		Setup_obtain_params_hover const prev_hover { _setup_obtain_params_hover };
		Setup_obtain_params_hover       next_hover { Setup_obtain_params_hover::NONE };

		node.with_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_sub_node("vbox", [&] (Xml_node const &node_2) {

					node_2.with_sub_node("float", [&] (Xml_node const &node_3) {
						if (node_3.attribute_value("name", String<3>()) == "ok") {
							next_hover = Setup_obtain_params_hover::START_BUTTON;
						}
					});

					node_2.with_sub_node("frame", [&] (Xml_node const &node_3) {
						if (node_3.attribute_value("name", String<4>()) == "pw1") {
							next_hover = Setup_obtain_params_hover::PASSPHRASE_1_INPUT;
						}
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_setup_obtain_params_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::CONTROLS_ROOT:
	{
		Controls_root_hover const prev_hover { _controls_root_hover };
		Controls_root_hover       next_hover { Controls_root_hover::NONE };

		node.with_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_sub_node("vbox", [&] (Xml_node const &node_2) {
					node_2.with_sub_node("hbox", [&] (Xml_node const &node_3) {
						node_3.with_sub_node("button", [&] (Xml_node const &node_4) {

							if (node_4.attribute_value("name", String<10>()) == "Shut down") {

								next_hover = Controls_root_hover::SHUT_DOWN_BUTTON;

							}
						});
					});
					node_2.with_sub_node("frame", [&] (Xml_node const &node_3) {
						node_3.with_sub_node("vbox", [&] (Xml_node const &node_4) {
							node_4.with_sub_node("vbox", [&] (Xml_node const &node_5) {

								if (node_5.attribute_value("name", String<10>()) == "Snapshots") {

									node_5.with_sub_node("float", [&] (Xml_node const &) {

										next_hover = Controls_root_hover::SNAPSHOTS_EXPAND_BUTTON;
									});
								} else if (node_5.attribute_value("name", String<11>()) == "Dimensions") {

									node_5.with_sub_node("float", [&] (Xml_node const &) {

										next_hover = Controls_root_hover::DIMENSIONS_EXPAND_BUTTON;
									});
								} else if (node_5.attribute_value("name", String<11>()) == "Security") {

									node_5.with_sub_node("float", [&] (Xml_node const &) {

										next_hover = Controls_root_hover::SECURITY_EXPAND_BUTTON;
									});
								}
							});
						});
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_controls_root_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::CONTROLS_SNAPSHOTS:
	{
		Controls_snapshots_hover const prev_hover { _controls_snapshots_hover };
		Controls_snapshots_hover       next_hover { Controls_snapshots_hover::NONE };

		Snapshot_pointer const prev_snapshots_hover { _snapshots_hover };
		Snapshot_pointer       next_snapshots_hover { };

		node.with_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_sub_node("vbox", [&] (Xml_node const &node_2) {
					node_2.with_sub_node("hbox", [&] (Xml_node const &node_3) {

						node_3.with_sub_node("button", [&] (Xml_node const &node_4) {

							if (node_4.attribute_value("name", String<10>()) == "Shut down") {

								next_hover = Controls_snapshots_hover::SHUT_DOWN_BUTTON;

							}
						});
					});
					node_2.with_sub_node("frame", [&] (Xml_node const &node_3) {
						node_3.with_sub_node("vbox", [&] (Xml_node const &node_4) {
							node_4.with_sub_node("vbox", [&] (Xml_node const &node_5) {
								node_5.with_sub_node("float", [&] (Xml_node const &node_6) {

									if (node_6.attribute_value("name", String<8>()) == "expand") {

										next_hover = Controls_snapshots_hover::SNAPSHOTS_EXPAND_BUTTON;

									} else if (node_6.attribute_value("name", String<8>()) == "discard") {

										next_hover = Controls_snapshots_hover::DISCARD_SNAPSHOT_BUTTON;

									} else if (node_6.attribute_value("name", String<9>()) == "create") {

										next_hover = Controls_snapshots_hover::CREATE_SNAPSHOT_BUTTON;

									} else {

										Generation const generation {
											node_6.attribute_value(
												"name", Generation { INVALID_GENERATION }) };

										if (generation != INVALID_GENERATION) {

											_snapshots.for_each([&] (Snapshot const &snap)
											{
												if (generation == snap.generation()) {
													next_snapshots_hover = snap;
												}
											});
										}
									}
								});
								
							});
						});
					});
				});
			});
		});
		if (next_snapshots_hover != prev_snapshots_hover) {

			_snapshots_hover = next_snapshots_hover;
			update_dialog = true;
		}
		if (next_hover != prev_hover) {

			_controls_snapshots_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::CONTROLS_DIMENSIONS:
	{
		Controls_dimensions_hover const prev_hover { _controls_dimensions_hover };
		Controls_dimensions_hover       next_hover { Controls_dimensions_hover::NONE };

		node.with_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_sub_node("vbox", [&] (Xml_node const &node_2) {
					node_2.with_sub_node("hbox", [&] (Xml_node const &node_3) {

						node_3.with_sub_node("button", [&] (Xml_node const &node_4) {

							if (node_4.attribute_value("name", String<10>()) == "Shut down") {

								next_hover = Controls_dimensions_hover::SHUT_DOWN_BUTTON;

							}
						});
					});
					node_2.with_sub_node("frame", [&] (Xml_node const &node_3) {
						node_3.with_sub_node("vbox", [&] (Xml_node const &node_4) {
							node_4.with_sub_node("vbox", [&] (Xml_node const &node_5) {
								node_5.with_sub_node("float", [&] (Xml_node const &node_6) {

									if (node_6.attribute_value("name", String<8>()) == "expand") {

										next_hover = Controls_dimensions_hover::DIMENSIONS_EXPAND_BUTTON;

									} else if (node_6.attribute_value("name", String<8>()) == "Start") {

										next_hover = Controls_dimensions_hover::RESIZING_START_BUTTON;
									}
								});
								node_5.with_sub_node("frame", [&] (Xml_node const &node_6) {

									if (node_6.attribute_value("name", String<8>()) == "blks") {
										next_hover = Controls_dimensions_hover::RESIZING_NR_OF_BLKS_INPUT;
									}
								});
							});
						});
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_controls_dimensions_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::CONTROLS_SECURITY:
	{
		Controls_security_hover const prev_hover { _controls_security_hover };
		Controls_security_hover       next_hover { Controls_security_hover::NONE };

		node.with_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_sub_node("vbox", [&] (Xml_node const &node_2) {
					node_2.with_sub_node("hbox", [&] (Xml_node const &node_3) {
						node_3.with_sub_node("button", [&] (Xml_node const &node_4) {

							if (node_4.attribute_value("name", String<10>()) == "Shut down") {

								next_hover = Controls_security_hover::SHUT_DOWN_BUTTON;

							}
						});
					});
					node_2.with_sub_node("frame", [&] (Xml_node const &node_3) {
						node_3.with_sub_node("vbox", [&] (Xml_node const &node_4) {
							node_4.with_sub_node("float", [&] (Xml_node const &node_5) {

								if (node_5.attribute_value("name", String<8>()) == "expand") {

									next_hover = Controls_security_hover::SECURITY_EXPAND_BUTTON;
								}
							});
							node_4.with_sub_node("vbox", [&] (Xml_node const &node_5) {

								if (node_5.attribute_value("name", String<32>()) == "Block Encryption Key") {

									node_5.with_sub_node("float", [&] (Xml_node const &) {

										next_hover = Controls_security_hover::BLOCK_ENCRYPTION_KEY_EXPAND_BUTTON;
									});
								} else if (node_5.attribute_value("name", String<32>()) == "Master Key") {

									node_5.with_sub_node("float", [&] (Xml_node const &) {

										next_hover = Controls_security_hover::MASTER_KEY_EXPAND_BUTTON;
									});
								} else if (node_5.attribute_value("name", String<32>()) == "User Passphrase") {

									node_5.with_sub_node("float", [&] (Xml_node const &) {

										next_hover = Controls_security_hover::USER_PASSPHRASE_EXPAND_BUTTON;
									});
								}
							});
						});
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_controls_security_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	{
		Controls_security_block_encryption_key_hover const prev_hover { _controls_security_block_encryption_key_hover };
		Controls_security_block_encryption_key_hover       next_hover { Controls_security_block_encryption_key_hover::NONE };

		node.with_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_sub_node("vbox", [&] (Xml_node const &node_2) {
					node_2.with_sub_node("hbox", [&] (Xml_node const &node_3) {
						node_3.with_sub_node("button", [&] (Xml_node const &node_4) {

							if (node_4.attribute_value("name", String<10>()) == "Shut down") {

								next_hover = Controls_security_block_encryption_key_hover::SHUT_DOWN_BUTTON;

							}
						});
					});
					node_2.with_sub_node("frame", [&] (Xml_node const &node_3) {
						node_3.with_sub_node("vbox", [&] (Xml_node const &node_4) {
							node_4.with_sub_node("button", [&] (Xml_node const &node_5) {

								if (node_5.attribute_value("name", String<8>()) == "Rekey") {

									next_hover = Controls_security_block_encryption_key_hover::REPLACE_BUTTON;
								}
							});
							node_4.with_sub_node("float", [&] (Xml_node const &node_5) {

								if (node_5.attribute_value("name", String<8>()) == "expand") {

									next_hover = Controls_security_block_encryption_key_hover::LEAVE_BUTTON;
								}
							});
						});
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_controls_security_block_encryption_key_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::CONTROLS_SECURITY_MASTER_KEY:
	{
		Controls_security_master_key_hover const prev_hover { _controls_security_master_key_hover };
		Controls_security_master_key_hover       next_hover { Controls_security_master_key_hover::NONE };

		node.with_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_sub_node("vbox", [&] (Xml_node const &node_2) {
					node_2.with_sub_node("hbox", [&] (Xml_node const &node_3) {
						node_3.with_sub_node("button", [&] (Xml_node const &node_4) {

							if (node_4.attribute_value("name", String<10>()) == "Shut down") {

								next_hover = Controls_security_master_key_hover::SHUT_DOWN_BUTTON;

							}
						});
					});
					node_2.with_sub_node("frame", [&] (Xml_node const &node_3) {
						node_3.with_sub_node("vbox", [&] (Xml_node const &node_4) {
							node_4.with_sub_node("float", [&] (Xml_node const &node_5) {

								if (node_5.attribute_value("name", String<8>()) == "expand") {

									next_hover = Controls_security_master_key_hover::LEAVE_BUTTON;
								}
							});
						});
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_controls_security_master_key_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:
	{
		Controls_security_user_passphrase_hover const prev_hover { _controls_security_user_passphrase_hover };
		Controls_security_user_passphrase_hover       next_hover { Controls_security_user_passphrase_hover::NONE };

		node.with_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_sub_node("vbox", [&] (Xml_node const &node_2) {
					node_2.with_sub_node("hbox", [&] (Xml_node const &node_3) {
						node_3.with_sub_node("button", [&] (Xml_node const &node_4) {

							if (node_4.attribute_value("name", String<10>()) == "Shut down") {

								next_hover = Controls_security_user_passphrase_hover::SHUT_DOWN_BUTTON;

							}
						});
					});
					node_2.with_sub_node("frame", [&] (Xml_node const &node_3) {
						node_3.with_sub_node("vbox", [&] (Xml_node const &node_4) {
							node_4.with_sub_node("float", [&] (Xml_node const &node_5) {

								if (node_5.attribute_value("name", String<8>()) == "expand") {

									next_hover = Controls_security_user_passphrase_hover::LEAVE_BUTTON;
								}
							});
						});
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_controls_security_user_passphrase_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	default:

		break;
	}
	if (update_dialog) {
		_dialog.trigger_update();
	}
}


/***********************
 ** Genode::Component **
 ***********************/

void Component::construct(Genode::Env &env)
{
	static Cbe_manager::Main main { env };
}
