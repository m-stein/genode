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
#include <passphrase.h>
#include <utf8.h>
#include <child_exit_state.h>
#include <menu_view_dialog.h>

namespace Cbe_manager {

	class Snapshot;
	class Main;

	template <typename T>
	class Const_pointer
	{
		private:

			T const *_obj;

		public:

			struct Invalid : Genode::Exception { };

			Const_pointer() : _obj { nullptr } { }

			Const_pointer(T const &obj) : _obj { &obj } { }

			T const &obj() const
			{
				if (_obj == nullptr)
					throw Invalid();

				return *_obj;
			}

			bool valid() const { return _obj != nullptr; }

			bool operator != (Const_pointer const &other) const
			{
				if (valid() != other.valid()) {
					return true;
				}
				if (valid()) {
					return _obj != other._obj;
				}
				return false;
			}
	};
}


class Cbe_manager::Snapshot
{
	private:

		Generation const _generation;

	public:

		Snapshot(Generation const &generation)
		:
			_generation { generation }
		{ }

		virtual ~Snapshot() { }


		/***************
		 ** Accessors **
		 ***************/

		Generation const &generation() const { return _generation; }
};


class Cbe_manager::Main
:
	private Sandbox::Local_service_base::Wakeup,
	private Sandbox::State_handler,
	private Gui::Input_event_handler,
	private Dynamic_rom_session::Xml_producer
{
	private:

		enum {
			STATE_STRING_CAPACITY = 2,
			CBE_BLOCK_SIZE = 4096,
			MAIN_FRAME_WIDTH = 70,
			INIT_CBE_NR_OF_LEVELS = 6,
			INIT_CBE_NR_OF_CHILDREN = 64,
			INIT_CBE_NR_OF_SUPERBLOCKS = 8,
		};

		using Report_service     = Sandbox::Local_service<Report::Session_component>;
		using Gui_service        = Sandbox::Local_service<Gui::Session_component>;
		using Rom_service        = Sandbox::Local_service<Dynamic_rom_session>;
		using Xml_report_handler = Report::Session_component::Xml_handler<Main>;
		using State_string       = String<STATE_STRING_CAPACITY>;
		using Snapshot_registry  = Registry<Registered<Snapshot>>;

		enum class State
		{
			INVALID,
			INIT_TRUST_ANCHOR_SETTINGS,
			INIT_TRUST_ANCHOR_IN_PROGRESS,
			INIT_CBE_DEVICE_SETTINGS,
			INIT_CBE_DEVICE_CREATE_FILE,
			INIT_CBE_DEVICE_INIT_CBE,
			CBE_DEVICE_CONTROLS,
		};

		enum class Init_ta_settings_hover
		{
			NONE,
			PASSPHRASE_INPUT,
			START_BUTTON
		};

		enum class Init_ta_settings_select
		{
			NONE,
			PASSPHRASE_INPUT,
			START_BUTTON
		};

		enum class Init_cbe_settings_hover
		{
			NONE,
			PASSPHRASE_INPUT,
			SIZE_INPUT,
			START_BUTTON
		};

		enum class Init_cbe_settings_select
		{
			NONE,
			PASSPHRASE_INPUT,
			SIZE_INPUT,
			START_BUTTON
		};

		enum class Cbe_device_controls_selected
		{
			NONE,
			RESIZING_NR_OF_BLKS_INPUT,
			RESIZING_START_BUTTON,
			REKEYING_START_BUTTON,
			CREATE_SNAPSHOT_BUTTON,
			DISCARD_SNAPSHOT_BUTTON,
		};

		enum class Cbe_device_controls_hovered
		{
			NONE,
			RESIZING_NR_OF_BLKS_INPUT,
			RESIZING_START_BUTTON,
			REKEYING_START_BUTTON,
			CREATE_SNAPSHOT_BUTTON,
			DISCARD_SNAPSHOT_BUTTON,
		};

		enum class Cbe_device_controls_resizing_state
		{
			INACTIVE,
			WAIT_TILL_DEVICE_IS_READY,
			ISSUE_REQUEST_AT_DEVICE,
			IN_PROGRESS_AT_DEVICE,
		};

		enum class Cbe_device_controls_rekeying_state
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

		Env                                   &_env;
		State                                  _state                            { State::INVALID };
		Heap                                   _heap                             { _env.ram(), _env.rm() };
		Attached_rom_dataspace                 _config                           { _env, "config" };
		Root_directory                         _vfs                              { _env, _heap, _config.xml().sub_node("vfs") };
		Registry<Child_state>                  _children                         { };
		Child_state                            _menu_view_child_state            { _children, "menu_view", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _fs_query_child_state             { _children, "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _rsz_fs_query_child_state         { _children, "rsz_fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _rky_fs_query_child_state         { _children, "rky_fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _cbe_init_trust_anchor            { _children, "cbe_init_trust_anchor", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _cbe_init_child_state             { _children, "cbe_init", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _vfs_child_state                  { _children, "vfs", Ram_quota { 64 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _vfs_block_child_state            { _children, "vfs_block", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _rsz_fs_tool_child_state          { _children, "rsz_fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _rky_fs_tool_child_state          { _children, "rky_fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _create_snap_fs_tool_child_state  { _children, "create_snap_fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _discard_snap_fs_tool_child_state { _children, "discard_snap_fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Xml_report_handler                     _fs_query_listing_handler         { *this, &Main::_handle_fs_query_listing };
		Xml_report_handler                     _rsz_fs_query_listing_handler     { *this, &Main::_handle_rsz_fs_query_listing };
		Xml_report_handler                     _rky_fs_query_listing_handler     { *this, &Main::_handle_rky_fs_query_listing };
		Sandbox                                _sandbox                          { _env, *this };
		Gui_service                            _gui_service                      { _sandbox, *this };
		Rom_service                            _rom_service                      { _sandbox, *this };
		Report_service                         _report_service                   { _sandbox, *this };
		Xml_report_handler                     _hover_handler                    { *this, &Main::_handle_hover };
		Constructible<Watch_handler<Main>>     _watch_handler                    { };
		Constructible<Expanding_reporter>      _clipboard_reporter               { };
		Constructible<Attached_rom_dataspace>  _clipboard_rom                    { };
		bool                                   _initial_config                   { true };
		Signal_handler<Main>                   _config_handler                   { _env.ep(), *this, &Main::_handle_config };
		Signal_handler<Main>                   _state_handler                    { _env.ep(), *this, &Main::_handle_state };
		Dynamic_rom_session                    _dialog                           { _env.ep(), _env.ram(), _env.rm(), *this };
		Passphrase                             _init_ta_setg_passphrase          { };
		Init_ta_settings_hover                 _init_ta_setg_hover               { Init_ta_settings_hover::NONE };
		Init_ta_settings_select                _init_ta_setg_select              { Init_ta_settings_hover::PASSPHRASE_INPUT };
		Passphrase                             _init_cbe_setg_passphrase         { };
		Passphrase                             _init_cbe_setg_size               { };
		Init_cbe_settings_hover                _init_cbe_setg_hover              { Init_ta_settings_hover::NONE };
		Init_cbe_settings_select               _init_cbe_setg_select             { Init_ta_settings_hover::PASSPHRASE_INPUT };
		Cbe_device_controls_hovered            _cbe_ctl_hover                    { Cbe_device_controls_selected::NONE };
		Cbe_device_controls_selected           _cbe_ctl_select                   { Cbe_device_controls_hovered::RESIZING_NR_OF_BLKS_INPUT };
		Cbe_device_controls_rekeying_state     _cbe_ctl_rky_state                { Cbe_device_controls_rekeying_state::INACTIVE };
		Cbe_device_controls_resizing_state     _cbe_ctl_rsz_state                { Cbe_device_controls_resizing_state::INACTIVE };
		Create_snapshot_state                  _create_snap_state                { Create_snapshot_state::INACTIVE };
		Discard_snapshot_state                 _discard_snap_state               { Discard_snapshot_state::INACTIVE };
		Generation                             _discard_snap_gen                 { INVALID_GENERATION };
		Passphrase                             _cbe_ctl_rsz_nr_of_blks           { };
		Snapshot_registry                      _snap_registry                    { };
		Const_pointer<Snapshot>                _hovered_snap                     { };
		Const_pointer<Snapshot>                _selected_snap                    { };

		void _gen_rsz_fs_query_start_node(Xml_generator &xml) const;

		void _gen_rky_fs_query_start_node(Xml_generator &xml) const;

		static bool _child_finished(Xml_node    const &sandbox_state,
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

		void _generate_default_sandbox_config(Xml_generator &xml) const;

		void _handle_fs_query_listing(Xml_node const &node);

		void _handle_rsz_fs_query_listing(Xml_node const &node);

		void _handle_rky_fs_query_listing(Xml_node const &node);

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
	if (str == "0") { return State::INVALID; }
	if (str == "1") { return State::INIT_TRUST_ANCHOR_SETTINGS; }
	if (str == "2") { return State::INIT_TRUST_ANCHOR_IN_PROGRESS; }
	if (str == "3") { return State::INIT_CBE_DEVICE_SETTINGS; }
	if (str == "4") { return State::INIT_CBE_DEVICE_CREATE_FILE; }
	if (str == "5") { return State::INIT_CBE_DEVICE_INIT_CBE; }
	if (str == "6") { return State::CBE_DEVICE_CONTROLS; }
	class Invalid_state_string { };
	throw Invalid_state_string { };
}


Main::State_string Main::_state_to_string(State state)
{
	switch (state) {
	case State::INVALID:                       return "0";
	case State::INIT_TRUST_ANCHOR_SETTINGS:    return "1";
	case State::INIT_TRUST_ANCHOR_IN_PROGRESS: return "2";
	case State::INIT_CBE_DEVICE_SETTINGS:      return "3";
	case State::INIT_CBE_DEVICE_CREATE_FILE:   return "4";
	case State::INIT_CBE_DEVICE_INIT_CBE:      return "5";
	case State::CBE_DEVICE_CONTROLS:           return "6";
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
		New_file new_file(_vfs, Directory::Path("/cbe/state"));
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


void Main::_handle_rsz_fs_query_listing(Xml_node const &node)
{
	log("--- rsz_fs_query ---");
	log(node);
	log("----------------");

	switch (_state) {
	case State::CBE_DEVICE_CONTROLS:

		switch (_cbe_ctl_rsz_state) {
		case Cbe_device_controls_resizing_state::WAIT_TILL_DEVICE_IS_READY:

			if (cbe_control_file_yields_state_idle(node, "extend")) {

				_cbe_ctl_rsz_state = Cbe_device_controls_resizing_state::ISSUE_REQUEST_AT_DEVICE;
				Signal_transmitter(_state_handler).submit();
			}
			break;

		case Cbe_device_controls_resizing_state::IN_PROGRESS_AT_DEVICE:

			if (cbe_control_file_yields_state_idle(node, "extend")) {

				_cbe_ctl_rsz_nr_of_blks = Passphrase { };
				_cbe_ctl_rsz_state = Cbe_device_controls_resizing_state::INACTIVE;
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


void Main::_handle_rky_fs_query_listing(Xml_node const &node)
{
	log("--- rky_fs_query ---");
	log(node);
	log("----------------");

	switch (_state) {
	case State::CBE_DEVICE_CONTROLS:

		switch (_cbe_ctl_rky_state) {
		case Cbe_device_controls_rekeying_state::WAIT_TILL_DEVICE_IS_READY:

			if (cbe_control_file_yields_state_idle(node, "rekey")) {

				_cbe_ctl_rky_state = Cbe_device_controls_rekeying_state::ISSUE_REQUEST_AT_DEVICE;
				Signal_transmitter(_state_handler).submit();
			}
			break;

		case Cbe_device_controls_rekeying_state::IN_PROGRESS_AT_DEVICE:

			if (cbe_control_file_yields_state_idle(node, "rekey")) {

				_cbe_ctl_rky_state = Cbe_device_controls_rekeying_state::INACTIVE;
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
	log("--- fs_query ---");
	log(node);
	log("----------------");

	switch (_state) {
	case State::INVALID:
	{
		State const state { _state_from_fs_query_listing(node) };
		if (state == State::INVALID) {

			_write_to_state_file(State::INIT_TRUST_ANCHOR_SETTINGS);

		} else {

			_state = state;
			Signal_transmitter(_state_handler).submit();
		}
		break;
	}
	case State::CBE_DEVICE_CONTROLS:
	{
		bool update_dialog { false };
		node.with_sub_node("dir", [&] (Xml_node const &node_0) {

			_snap_registry.for_each([&] (Snapshot const &snap)
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

					if (_selected_snap.valid() &&
					    &_selected_snap.obj() == &snap) {

						_selected_snap = Const_pointer<Snapshot> { };
					}
					if (_hovered_snap.valid() &&
					    &_hovered_snap.obj() == &snap) {

						_hovered_snap = Const_pointer<Snapshot> { };
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
				_snap_registry.for_each([&] (Snapshot const &snap)
				{
					if (generation == snap.generation()) {
						snap_already_exists = true;
					}
				});
				if (!snap_already_exists) {
					new (_heap) Registered<Snapshot>(_snap_registry, generation);
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


bool Cbe_manager::Main::_child_finished(Xml_node    const &sandbox_state,
                                        Child_state const &child_state)
{
	Child_exit_state const exit_state { sandbox_state, child_state.name() };

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
	/* obtain current sandbox state */
	Buffered_xml sandbox_state(_heap, "sandbox_state", [&] (Xml_generator &xml) {
		_sandbox.generate_state_report(xml);
	});

	bool update_sandbox { false };
	bool update_dialog { false };
	sandbox_state.with_xml_node([&] (Xml_node const &sandbox_state) {

		switch (_state) {
		case State::INIT_TRUST_ANCHOR_IN_PROGRESS:

			if (_child_finished(sandbox_state, _cbe_init_trust_anchor)) {

				_state = State::INIT_CBE_DEVICE_SETTINGS;
				_init_cbe_setg_passphrase = _init_ta_setg_passphrase;
				_init_cbe_setg_select = Init_cbe_settings_select::SIZE_INPUT;
				update_dialog = true;
				update_sandbox = true;
			}
			break;

		case State::INIT_CBE_DEVICE_INIT_CBE:

			if (_child_finished(sandbox_state, _cbe_init_child_state)) {

				_state = State::CBE_DEVICE_CONTROLS;
				update_dialog = true;
				update_sandbox = true;
			}
			break;

		case State::CBE_DEVICE_CONTROLS:

			switch (_cbe_ctl_rsz_state) {
			case Cbe_device_controls_resizing_state::ISSUE_REQUEST_AT_DEVICE:

				if (_child_finished(sandbox_state, _rsz_fs_tool_child_state)) {

					_cbe_ctl_rsz_state = Cbe_device_controls_resizing_state::IN_PROGRESS_AT_DEVICE;
					update_dialog = true;
					update_sandbox = true;
				}
				break;

			default:

				break;
			}

			switch (_cbe_ctl_rky_state) {
			case Cbe_device_controls_rekeying_state::ISSUE_REQUEST_AT_DEVICE:

				if (_child_finished(sandbox_state, _rky_fs_tool_child_state)) {

					_cbe_ctl_rky_state = Cbe_device_controls_rekeying_state::IN_PROGRESS_AT_DEVICE;
					update_dialog = true;
					update_sandbox = true;
				}
				break;

			default:

				break;
			}

			switch (_create_snap_state) {
			case Create_snapshot_state::ISSUE_REQUEST_AT_DEVICE:

				if (_child_finished(sandbox_state, _create_snap_fs_tool_child_state)) {

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

				if (_child_finished(sandbox_state, _discard_snap_fs_tool_child_state)) {

					_discard_snap_state = Discard_snapshot_state::INACTIVE;
					update_dialog = true;
					update_sandbox = true;
				}
				break;

			default:

				break;
			}

			break;

		default:

			break;
		}
		sandbox_state.for_each_sub_node("child", [&] (Xml_node const &child) {

			if (_fs_query_child_state.apply_child_state_report(child)) {
				update_sandbox = true;
			}
			if (_menu_view_child_state.apply_child_state_report(child)) {
				update_sandbox = true;
			}
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

		gen_titled_in_progress_frame(xml, "1", "Program initialization", MAIN_FRAME_WIDTH);
		break;

	case State::INIT_TRUST_ANCHOR_SETTINGS:

		gen_titled_frame(xml, "1", "Trust anchor initialization", MAIN_FRAME_WIDTH, [&] (Xml_generator &xml) {

			gen_titled_text_input(
				xml, "pw", "Trust anchor passphrase",
				_init_ta_setg_passphrase.blind(),
				_init_ta_setg_select == Init_ta_settings_select::PASSPHRASE_INPUT);

			bool gen_start_button { true };
			if (!_init_ta_setg_passphrase.suitable()) {

				gen_start_button = false;
				gen_info_line(xml, "info", _init_ta_setg_passphrase.not_suitable_text());
			}
			gen_info_line(xml, "pad", "");
			if (gen_start_button) {

				gen_action_button_at_bottom(
					xml, "ok", "Start",
					_init_ta_setg_hover == Init_ta_settings_hover::START_BUTTON,
					_init_ta_setg_select == Init_ta_settings_select::START_BUTTON);
			}
		});
		break;

	case State::INIT_TRUST_ANCHOR_IN_PROGRESS:

		gen_titled_in_progress_frame(xml, "1", "Trust anchor initialization", MAIN_FRAME_WIDTH);
		break;

	case State::INIT_CBE_DEVICE_SETTINGS:

		gen_titled_frame(xml, "1", "CBE device initialization", MAIN_FRAME_WIDTH, [&] (Xml_generator &xml) {

			gen_titled_text_input(
				xml, "pw", "Trust anchor passphrase",
				_init_cbe_setg_passphrase.blind(),
				_init_cbe_setg_select == Init_cbe_settings_select::PASSPHRASE_INPUT);

			bool gen_start_button { true };
			if (!_init_cbe_setg_passphrase.suitable()) {

				gen_start_button = false;
				gen_info_line(xml, "info_1", _init_cbe_setg_passphrase.not_suitable_text());
			}
			gen_info_line(xml, "pad_1", "");
			gen_titled_text_input(
				xml, "sz", "Device size (suffixes K, M, G)",
				_init_cbe_setg_size,
				_init_cbe_setg_select == Init_cbe_settings_select::SIZE_INPUT);

			if (!_init_cbe_setg_size.is_nr_of_bytes_greater_than_zero()) {

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
					_init_cbe_setg_hover == Init_cbe_settings_hover::START_BUTTON,
					_init_cbe_setg_select == Init_cbe_settings_select::START_BUTTON);
			}
		});
		break;

	case State::INIT_CBE_DEVICE_CREATE_FILE:

		gen_titled_in_progress_frame(xml, "1", "CBE device initialization", MAIN_FRAME_WIDTH);
		break;

	case State::INIT_CBE_DEVICE_INIT_CBE:

		gen_titled_in_progress_frame(xml, "1", "CBE device initialization", MAIN_FRAME_WIDTH);
		break;

	case State::CBE_DEVICE_CONTROLS:

		gen_titled_frame(xml, "app", "CBE device controls", MAIN_FRAME_WIDTH, [&] (Xml_generator &xml) {

			gen_floating_text_frame(xml, "inf", [&] (Xml_generator &xml) {

				gen_floating_text_line(xml, "line_1", "Device info:");
				gen_floating_text_line(xml, "line_2", "  Snapshots:");

				unsigned long snap_idx { 1 };
				_snap_registry.for_each([&] (Snapshot const &snap) {

					bool const selected {
						_selected_snap.valid() &&
						_selected_snap.obj().generation() == snap.generation() };

					String<128> const snap_str {
						" [", snap_idx++, "] Generation ", snap.generation() };

					gen_floating_text_line(
						xml,
						Generation_string { snap.generation() }.string(),
						String<128> {"   ", snap_str }.string(),
						selected ? 3 : 0,
						selected ? snap_str.length() : 0);
				});
			});

			xml.node("hbox", [&] () {

				gen_titled_frame(xml, "rky", "Rekeying", 30, [&] (Xml_generator &xml) {

					gen_info_line(xml, "pad_1", "");

					switch(_cbe_ctl_rky_state) {
					case Cbe_device_controls_rekeying_state::INACTIVE:

						gen_action_button_at_bottom(xml, "Start",
							_cbe_ctl_hover  == Cbe_device_controls_hovered::REKEYING_START_BUTTON,
							_cbe_ctl_select == Cbe_device_controls_selected::REKEYING_START_BUTTON);

						break;

					case Cbe_device_controls_rekeying_state::WAIT_TILL_DEVICE_IS_READY:

						gen_info_line(xml, "inf", "Wait for device...");
						gen_info_line(xml, "pad_2", "");
						break;

					case Cbe_device_controls_rekeying_state::ISSUE_REQUEST_AT_DEVICE:

						gen_info_line(xml, "inf", "Initiate...");
						gen_info_line(xml, "pad_2", "");
						break;

					case Cbe_device_controls_rekeying_state::IN_PROGRESS_AT_DEVICE:

						gen_info_line(xml, "inf", "In progress...");
						gen_info_line(xml, "pad_2", "");
						break;
					}

					switch(_create_snap_state) {
					case Create_snapshot_state::INACTIVE:

						gen_action_button_at_bottom(xml, "Create snapshot",
							_cbe_ctl_hover  == Cbe_device_controls_hovered::CREATE_SNAPSHOT_BUTTON,
							_cbe_ctl_select == Cbe_device_controls_selected::CREATE_SNAPSHOT_BUTTON);

						break;

					case Create_snapshot_state::ISSUE_REQUEST_AT_DEVICE:

						gen_info_line(xml, "inf_3", "In progress...");
						gen_info_line(xml, "pad_3", "");
						break;
					}

					switch(_discard_snap_state) {
					case Discard_snapshot_state::INACTIVE:

						gen_action_button_at_bottom(xml, "Discard snapshot",
							_cbe_ctl_hover  == Cbe_device_controls_hovered::DISCARD_SNAPSHOT_BUTTON,
							_cbe_ctl_select == Cbe_device_controls_selected::DISCARD_SNAPSHOT_BUTTON);

						break;

					case Discard_snapshot_state::ISSUE_REQUEST_AT_DEVICE:

						gen_info_line(xml, "inf_4", "In progress...");
						gen_info_line(xml, "pad_4", "");
						break;
					}
				});

				gen_titled_frame(xml, "rsz", "Resizing", 30, [&] (Xml_generator &xml) {

					gen_info_line(xml, "pad_1", "");

					switch(_cbe_ctl_rsz_state) {
					case Cbe_device_controls_resizing_state::INACTIVE:
					{
						gen_titled_text_input(
							xml, "blks", "Number of blocks",
							_cbe_ctl_rsz_nr_of_blks,
							_cbe_ctl_select == Cbe_device_controls_selected::RESIZING_NR_OF_BLKS_INPUT);

						bool gen_start_button { true };
						if (!_cbe_ctl_rsz_nr_of_blks.is_nr_greater_than_zero()) {

							gen_start_button = false;
							gen_info_line(xml, "inf", "Must be a number greater than 0");
							gen_info_line(xml, "pad_2", "");

						}  else {

							Number_of_bytes const curr_cbe_size { _cbe_size() };
							unsigned long rsz_nr_of_bytes {
								_cbe_ctl_rsz_nr_of_blks.to_unsigned_long() *
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

							gen_info_line(xml, "pad_2", "");
						}
						if (gen_start_button) {

							gen_action_button_at_bottom(
								xml, "Start",
								_cbe_ctl_hover  == Cbe_device_controls_hovered::RESIZING_START_BUTTON,
								_cbe_ctl_select == Cbe_device_controls_selected::RESIZING_START_BUTTON);
						}
						break;
					}
					case Cbe_device_controls_resizing_state::WAIT_TILL_DEVICE_IS_READY:

						gen_info_line(xml, "inf", "Wait for device...");
						gen_info_line(xml, "pad_2", "");
						break;

					case Cbe_device_controls_resizing_state::ISSUE_REQUEST_AT_DEVICE:

						gen_info_line(xml, "inf", "Initiate...");
						gen_info_line(xml, "pad_2", "");
						break;

					case Cbe_device_controls_resizing_state::IN_PROGRESS_AT_DEVICE:

						gen_info_line(xml, "inf", "In progress...");
						gen_info_line(xml, "pad_2", "");
						break;
					}
				});
			});
		});
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

		} else if (request.label == "rsz_fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _rsz_fs_query_listing_handler, _env.ep(),
					request.resources, "", request.diag) };

			request.deliver_session(session);

		} else if (request.label == "rky_fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _rky_fs_query_listing_handler, _env.ep(),
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


void Main::_generate_default_sandbox_config(Xml_generator &xml) const
{
	xml.attribute("verbose",  "yes");

	xml.node("report", [&] () {
		xml.attribute("child_ram",  "yes");
		xml.attribute("child_caps", "yes");
		xml.attribute("delay_ms", 500);
	});

	xml.node("parent-provides", [&] () {
		service_node(xml, "ROM");
		service_node(xml, "CPU");
		service_node(xml, "PD");
		service_node(xml, "LOG");
		service_node(xml, "File_system");
		service_node(xml, "Gui");
		service_node(xml, "Timer");
		service_node(xml, "Report");
	});

	xml.node("start", [&] () {
		_menu_view_child_state.gen_start_node_content(xml);

		xml.node("config", [&] () {
			xml.attribute("xpos", "100");
			xml.attribute("ypos", "50");

			xml.node("report", [&] () {
				xml.attribute("hover", "yes"); });

			xml.node("libc", [&] () {
				xml.attribute("stderr", "/dev/log"); });

			xml.node("vfs", [&] () {
				xml.node("tar", [&] () {
					xml.attribute("name", "menu_view_styles.tar"); });
				xml.node("dir", [&] () {
					xml.attribute("name", "dev");
					xml.node("log", [&] () { });
				});
				xml.node("dir", [&] () {
					xml.attribute("name", "fonts");
					xml.node("fs", [&] () {
						xml.attribute("label", "fonts");
					});
				});
			});
		});

		xml.node("route", [&] () {
			route_to_local_service(xml, "ROM", "dialog");
			route_to_local_service(xml, "Report", "hover");
			route_to_local_service(xml, "Gui");
			route_to_parent_service(xml, "File_system", "fonts");
			route_to_parent_service(xml, "Timer");
			route_to_parent_service(xml, "PD");
			route_to_parent_service(xml, "ROM");
			route_to_parent_service(xml, "CPU");
			route_to_parent_service(xml, "LOG");
		});
	});
}


size_t Main::_init_cbe_nr_of_leafs() const
{
	size_t const size { _init_cbe_setg_size.to_nr_of_bytes() };
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

		_generate_default_sandbox_config(xml);

		xml.node("start", [&] () {

			_fs_query_child_state.gen_start_node_content(xml);
			xml.node("config", [&] () {
				xml.node("vfs", [&] () {
					xml.node("fs", [&] () {
						xml.attribute("writeable", "yes");
					});
				});
				xml.node("query", [&] () {
					xml.attribute("path", "/");
					xml.attribute("content", "yes");
				});
			});
			xml.node("route", [&] () {
				route_to_local_service(xml, "Report");
				route_to_parent_service(xml, "File_system");
				route_to_parent_service(xml, "PD");
				route_to_parent_service(xml, "ROM");
				route_to_parent_service(xml, "CPU");
				route_to_parent_service(xml, "LOG");
			});
		});
		break;

	case State::INIT_TRUST_ANCHOR_SETTINGS:

		_generate_default_sandbox_config(xml);
		break;

	case State::INIT_TRUST_ANCHOR_IN_PROGRESS:

		_generate_default_sandbox_config(xml);
		xml.node("start", [&] () {

			_cbe_init_trust_anchor.gen_start_node_content(xml);
			xml.node("config", [&] () {

				String<_init_ta_setg_passphrase.MAX_LENGTH * 3> const
					passphrase {
						_init_ta_setg_passphrase };

				xml.attribute("passphrase", passphrase.string());
				xml.attribute("trust_anchor_dir", "/trust_anchor");
				xml.node("vfs", [&] () {
					xml.node("dir", [&] () {
						xml.attribute("name", "trust_anchor");
						xml.node("fs", [&] () {
							xml.attribute("label", "trust_anchor");
						});
					});
				});
			});
			xml.node("route", [&] () {
				route_to_parent_service(xml, "File_system", "trust_anchor");
				route_to_parent_service(xml, "PD");
				route_to_parent_service(xml, "ROM");
				route_to_parent_service(xml, "CPU");
				route_to_parent_service(xml, "LOG");
			});
		});
		break;

	case State::INIT_CBE_DEVICE_SETTINGS:

		_generate_default_sandbox_config(xml);
		break;

	case State::INIT_CBE_DEVICE_CREATE_FILE:

		_generate_default_sandbox_config(xml);
		break;

	case State::INIT_CBE_DEVICE_INIT_CBE:

		_generate_default_sandbox_config(xml);

		xml.node("start", [&] () {

			_vfs_block_child_state.gen_start_node_content(xml);
			xml.node("provides", [&] () {
				xml.node("service", [&] () {
					xml.attribute("name", "Block");
				});
			});
			xml.node("config", [&] () {
				xml.node("vfs", [&] () {
					xml.node("fs", [&] () {
						xml.attribute("buffer_size", "1M");
					});
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "cbe_init -> ");
					xml.attribute("block_size", "512");
					xml.attribute("file", "/cbe.img");
					xml.attribute("writeable", "yes");
				});
			});
			xml.node("route", [&] () {
				route_to_parent_service(xml, "File_system");
				route_to_parent_service(xml, "PD");
				route_to_parent_service(xml, "ROM");
				route_to_parent_service(xml, "CPU");
				route_to_parent_service(xml, "LOG");
			});
		});

		xml.node("start", [&] () {

			_cbe_init_child_state.gen_start_node_content(xml);
			xml.node("config", [&] () {
				xml.attribute("trust_anchor_dir", "/trust_anchor");

				xml.node("vfs", [&] () {
					xml.node("dir", [&] () {
						xml.attribute("name", "trust_anchor");
						xml.node("fs", [&] () {
							xml.attribute("label", "trust_anchor");
						});
					});
				});

				xml.node("key", [&] () {
					xml.attribute("id", "12");
				});

				xml.node("virtual-block-device", [&] () {
					xml.attribute("nr_of_levels", INIT_CBE_NR_OF_LEVELS);
					xml.attribute("nr_of_children", INIT_CBE_NR_OF_CHILDREN);
					xml.attribute("nr_of_leafs", _init_cbe_nr_of_leafs());
				});

				xml.node("free-tree", [&] () {
					xml.attribute("nr_of_levels", INIT_CBE_NR_OF_LEVELS);
					xml.attribute("nr_of_children", INIT_CBE_NR_OF_CHILDREN);
					xml.attribute("nr_of_leafs", _init_cbe_nr_of_leafs());
				});
			});
			xml.node("route", [&] () {
				route_to_parent_service(xml, "File_system", "trust_anchor");
				route_to_child_service(xml, "vfs_block", "Block");
				route_to_parent_service(xml, "PD");
				route_to_parent_service(xml, "ROM");
				route_to_parent_service(xml, "CPU");
				route_to_parent_service(xml, "LOG");
			});
		});
		break;

	case State::CBE_DEVICE_CONTROLS:

		_generate_default_sandbox_config(xml);

		xml.node("start", [&] () {
			_vfs_child_state.gen_start_node_content(xml);

			xml.node("provides", [&] () {
				xml.node("service", [&] () {
					xml.attribute("name", "File_system");
				});
			});
			xml.node("config", [&] () {

				xml.node("vfs", [&] () {

					xml.node("fs", [&] () {
						xml.attribute("buffer_size", "1M");
						xml.attribute("label", "cbe_fs");
					});
					xml.node("cbe_crypto_aes_cbc", [&] () {
						xml.attribute("name", "crypto");
					});
					xml.node("dir", [&] () {
						xml.attribute("name", "trust_anchor");

						xml.node("fs", [&] () {
							xml.attribute("buffer_size", "1M");
							xml.attribute("label", "trust_anchor");
						});
					});
					xml.node("dir", [&] () {
						xml.attribute("name", "dev");

						xml.node("cbe", [&] () {
							xml.attribute("name", "cbe");
							xml.attribute("verbose", "yes");
							xml.attribute("debug", "no");
							xml.attribute("block", "/cbe.img");
							xml.attribute("crypto", "/crypto");
							xml.attribute("trust_anchor", "/trust_anchor");
						});
					});
				});

				xml.node("policy", [&] () {
					xml.attribute("label", "rsz_fs_tool -> ");
					xml.attribute("root", "/dev");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "rky_fs_tool -> ");
					xml.attribute("root", "/dev");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "create_snap_fs_tool -> ");
					xml.attribute("root", "/dev");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "discard_snap_fs_tool -> ");
					xml.attribute("root", "/dev");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "fs_query -> ");
					xml.attribute("root", "/dev");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "rsz_fs_query -> ");
					xml.attribute("root", "/dev");
					xml.attribute("writeable", "yes");
				});
				xml.node("policy", [&] () {
					xml.attribute("label", "rky_fs_query -> ");
					xml.attribute("root", "/dev");
					xml.attribute("writeable", "yes");
				});
				xml.node("default-policy", [&] () {
					xml.attribute("root", "/dev/cbe/current");
					xml.attribute("writeable", "yes");
				});
			});
			xml.node("route", [&] () {
				route_to_parent_service(xml, "File_system", "cbe_fs");
				route_to_parent_service(xml, "File_system", "trust_anchor");
				route_to_parent_service(xml, "PD");
				route_to_parent_service(xml, "ROM");
				route_to_parent_service(xml, "CPU");
				route_to_parent_service(xml, "LOG");
			});
		});

		xml.node("start", [&] () {
			_fs_query_child_state.gen_start_node_content(xml);

			xml.node("config", [&] () {
				xml.node("vfs", [&] () {
					xml.node("fs", [&] () {
						xml.attribute("writeable", "yes");
					});
				});
				xml.node("query", [&] () {
					xml.attribute("path", "/cbe/snapshots");
					xml.attribute("content", "yes");
				});
			});
			xml.node("route", [&] () {
				route_to_local_service(xml, "Report");
				route_to_child_service(xml, "vfs", "File_system");
				route_to_parent_service(xml, "PD");
				route_to_parent_service(xml, "ROM");
				route_to_parent_service(xml, "CPU");
				route_to_parent_service(xml, "LOG");
			});
		});

		switch(_cbe_ctl_rsz_state) {
		case Cbe_device_controls_resizing_state::INACTIVE:

			break;

		case Cbe_device_controls_resizing_state::WAIT_TILL_DEVICE_IS_READY:

			_gen_rsz_fs_query_start_node(xml);
			break;

		case Cbe_device_controls_resizing_state::ISSUE_REQUEST_AT_DEVICE:

			xml.node("start", [&] () {
				_rsz_fs_tool_child_state.gen_start_node_content(xml);

				xml.node("binary", [&] () {
					xml.attribute("name", "fs_tool");
				});
				xml.node("config", [&] () {
					xml.attribute("exit",    "yes");
					xml.attribute("verbose", "yes");

					xml.node("vfs", [&] () {
						xml.node("dir", [&] () {
							xml.attribute("name", "cbe");

							xml.node("fs",  [&] () {
								xml.attribute("writeable", "yes");
							});
						});
					});
					xml.node("new-file", [&] () {
						xml.attribute("path", "/cbe/cbe/control/extend");
						xml.append_content(
							"tree=vbd,blocks=",
							_cbe_ctl_rsz_nr_of_blks.to_unsigned_long());
					});
				});
				xml.node("route", [&] () {

					route_to_child_service(xml, "vfs", "File_system");
					route_to_parent_service(xml, "PD");
					route_to_parent_service(xml, "ROM");
					route_to_parent_service(xml, "CPU");
					route_to_parent_service(xml, "LOG");
				});
			});
			break;

		case Cbe_device_controls_resizing_state::IN_PROGRESS_AT_DEVICE:

			_gen_rsz_fs_query_start_node(xml);
			break;
		}

		switch(_cbe_ctl_rky_state) {
		case Cbe_device_controls_rekeying_state::INACTIVE:

			break;

		case Cbe_device_controls_rekeying_state::WAIT_TILL_DEVICE_IS_READY:

			_gen_rky_fs_query_start_node(xml);
			break;

		case Cbe_device_controls_rekeying_state::ISSUE_REQUEST_AT_DEVICE:

			xml.node("start", [&] () {
				_rky_fs_tool_child_state.gen_start_node_content(xml);

				xml.node("binary", [&] () {
					xml.attribute("name", "fs_tool");
				});
				xml.node("config", [&] () {
					xml.attribute("exit",    "yes");
					xml.attribute("verbose", "yes");

					xml.node("vfs", [&] () {
						xml.node("dir", [&] () {
							xml.attribute("name", "cbe");

							xml.node("fs",  [&] () {
								xml.attribute("writeable", "yes");
							});
						});
					});
					xml.node("new-file", [&] () {
						xml.attribute("path", "/cbe/cbe/control/rekey");
						xml.append_content("true");
					});
				});
				xml.node("route", [&] () {

					route_to_child_service(xml, "vfs", "File_system");
					route_to_parent_service(xml, "PD");
					route_to_parent_service(xml, "ROM");
					route_to_parent_service(xml, "CPU");
					route_to_parent_service(xml, "LOG");
				});
			});
			break;

		case Cbe_device_controls_rekeying_state::IN_PROGRESS_AT_DEVICE:

			_gen_rky_fs_query_start_node(xml);
			break;
		}

		switch(_create_snap_state) {
		case Create_snapshot_state::INACTIVE:

			break;

		case Create_snapshot_state::ISSUE_REQUEST_AT_DEVICE:

			xml.node("start", [&] () {
				_create_snap_fs_tool_child_state.gen_start_node_content(xml);

				xml.node("binary", [&] () {
					xml.attribute("name", "fs_tool");
				});
				xml.node("config", [&] () {
					xml.attribute("exit",    "yes");
					xml.attribute("verbose", "yes");

					xml.node("vfs", [&] () {
						xml.node("dir", [&] () {
							xml.attribute("name", "cbe");

							xml.node("fs",  [&] () {
								xml.attribute("writeable", "yes");
							});
						});
					});
					xml.node("new-file", [&] () {
						xml.attribute("path", "/cbe/cbe/control/create_snapshot");
						xml.append_content("true");
					});
				});
				xml.node("route", [&] () {

					route_to_child_service(xml, "vfs", "File_system");
					route_to_parent_service(xml, "PD");
					route_to_parent_service(xml, "ROM");
					route_to_parent_service(xml, "CPU");
					route_to_parent_service(xml, "LOG");
				});
			});
			break;
		}

		switch(_discard_snap_state) {
		case Discard_snapshot_state::INACTIVE:

			break;

		case Discard_snapshot_state::ISSUE_REQUEST_AT_DEVICE:

			xml.node("start", [&] () {
				_discard_snap_fs_tool_child_state.gen_start_node_content(xml);

				xml.node("binary", [&] () {
					xml.attribute("name", "fs_tool");
				});
				xml.node("config", [&] () {
					xml.attribute("exit",    "yes");
					xml.attribute("verbose", "yes");

					xml.node("vfs", [&] () {
						xml.node("dir", [&] () {
							xml.attribute("name", "cbe");

							xml.node("fs",  [&] () {
								xml.attribute("writeable", "yes");
							});
						});
					});
					xml.node("new-file", [&] () {
						xml.attribute("path", "/cbe/cbe/control/discard_snapshot");
						xml.append_content(Generation_string(_discard_snap_gen));
					});
				});
				xml.node("route", [&] () {

					route_to_child_service(xml, "vfs", "File_system");
					route_to_parent_service(xml, "PD");
					route_to_parent_service(xml, "ROM");
					route_to_parent_service(xml, "CPU");
					route_to_parent_service(xml, "LOG");
				});
			});
			break;
		}

		break;
	}
}


void Cbe_manager::Main::_gen_rsz_fs_query_start_node(Xml_generator &xml) const
{
	xml.node("start", [&] () {
		_rsz_fs_query_child_state.gen_start_node_content(xml);

		xml.node("binary", [&] () {
			xml.attribute("name", "fs_query");
		});
		xml.node("config", [&] () {
			xml.node("vfs", [&] () {
				xml.node("fs", [&] () {
					xml.attribute("writeable", "yes");
				});
			});
			xml.node("query", [&] () {
				xml.attribute("path", "/cbe/control");
				xml.attribute("content", "yes");
			});
		});
		xml.node("route", [&] () {
			route_to_local_service(xml, "Report");
			route_to_child_service(xml, "vfs", "File_system");
			route_to_parent_service(xml, "PD");
			route_to_parent_service(xml, "ROM");
			route_to_parent_service(xml, "CPU");
			route_to_parent_service(xml, "LOG");
		});
	});
}


void Cbe_manager::Main::_gen_rky_fs_query_start_node(Xml_generator &xml) const
{
	xml.node("start", [&] () {
		_rky_fs_query_child_state.gen_start_node_content(xml);

		xml.node("binary", [&] () {
			xml.attribute("name", "fs_query");
		});
		xml.node("config", [&] () {
			xml.node("vfs", [&] () {
				xml.node("fs", [&] () {
					xml.attribute("writeable", "yes");
				});
			});
			xml.node("query", [&] () {
				xml.attribute("path", "/cbe/control");
				xml.attribute("content", "yes");
			});
		});
		xml.node("route", [&] () {
			route_to_local_service(xml, "Report");
			route_to_child_service(xml, "vfs", "File_system");
			route_to_parent_service(xml, "PD");
			route_to_parent_service(xml, "ROM");
			route_to_parent_service(xml, "CPU");
			route_to_parent_service(xml, "LOG");
		});
	});
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
	 * This would be the correct way to calculate the numer of MT blocks
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
	case State::INIT_TRUST_ANCHOR_SETTINGS:

		event.handle_press([&] (Input::Keycode key, Codepoint code) {

			if (key == Input::BTN_LEFT) {

				Init_ta_settings_select const prev_select { _init_ta_setg_select };
				Init_ta_settings_select       next_select { Init_ta_settings_select::NONE };

				switch (_init_ta_setg_hover) {
				case Init_ta_settings_hover::START_BUTTON:

					next_select = Init_ta_settings_select::START_BUTTON;
					break;

				case Init_ta_settings_hover::PASSPHRASE_INPUT:

					next_select = Init_ta_settings_select::PASSPHRASE_INPUT;
					break;

				case Init_ta_settings_hover::NONE:

					next_select = Init_ta_settings_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_init_ta_setg_select = next_select;
					update_dialog = true;
				}

			} else if (key == Input::KEY_ENTER) {

				if (_init_ta_setg_passphrase.suitable() &&
				    _init_ta_setg_select != Init_ta_settings_select::START_BUTTON) {

					_init_ta_setg_select = Init_ta_settings_select::START_BUTTON;
					update_dialog = true;
				}

			} else {

				if (_init_ta_setg_select == Init_ta_settings_select::PASSPHRASE_INPUT) {

					if (codepoint_is_printable(code)) {

						_init_ta_setg_passphrase.append_character(code);
						update_dialog = true;

					} else if (code.value == CODEPOINT_BACKSPACE) {

						_init_ta_setg_passphrase.remove_last_character();
						update_dialog = true;
					}
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT ||
			    key == Input::KEY_ENTER) {

				if (_init_ta_setg_passphrase.suitable() &&
				    _init_ta_setg_select == Init_ta_settings_select::START_BUTTON) {

					_init_ta_setg_select = Init_ta_settings_select::NONE;
					_state = State::INIT_TRUST_ANCHOR_IN_PROGRESS;
					update_sandbox_config = true;
					update_dialog = true;
				}
			}
		});
		break;

	case State::INIT_CBE_DEVICE_SETTINGS:

		event.handle_press([&] (Input::Keycode key, Codepoint code) {

			if (key == Input::BTN_LEFT) {

				Init_cbe_settings_select const prev_select { _init_cbe_setg_select };
				Init_cbe_settings_select       next_select { Init_cbe_settings_select::NONE };

				switch (_init_cbe_setg_hover) {
				case Init_cbe_settings_hover::START_BUTTON:

					next_select = Init_cbe_settings_select::START_BUTTON;
					break;

				case Init_cbe_settings_hover::PASSPHRASE_INPUT:

					next_select = Init_cbe_settings_select::PASSPHRASE_INPUT;
					break;

				case Init_cbe_settings_hover::SIZE_INPUT:

					next_select = Init_cbe_settings_select::SIZE_INPUT;
					break;

				case Init_cbe_settings_hover::NONE:

					next_select = Init_cbe_settings_select::NONE;
					break;
				}
				if (next_select != prev_select) {

					_init_cbe_setg_select = next_select;
					update_dialog = true;
				}

			} else if (key == Input::KEY_ENTER) {

				if (_init_cbe_setg_passphrase.suitable() &&
				    _init_cbe_setg_size.is_nr_of_bytes_greater_than_zero() &&
				    _init_cbe_setg_select != Init_cbe_settings_select::START_BUTTON) {

					_init_cbe_setg_select = Init_cbe_settings_select::START_BUTTON;
					update_dialog = true;
				}

			} else {

				if (_init_cbe_setg_select == Init_cbe_settings_select::PASSPHRASE_INPUT) {

					if (codepoint_is_printable(code)) {

						_init_cbe_setg_passphrase.append_character(code);
						update_dialog = true;

					} else if (code.value == CODEPOINT_BACKSPACE) {

						_init_cbe_setg_passphrase.remove_last_character();
						update_dialog = true;

					} else if (code.value == CODEPOINT_TAB) {

						_init_cbe_setg_select = Init_cbe_settings_select::SIZE_INPUT;
						update_dialog = true;
					}

				} else if (_init_cbe_setg_select == Init_cbe_settings_select::SIZE_INPUT) {

					if (codepoint_is_printable(code)) {

						_init_cbe_setg_size.append_character(code);
						update_dialog = true;

					} else if (code.value == CODEPOINT_BACKSPACE) {

						_init_cbe_setg_size.remove_last_character();
						update_dialog = true;

					} else if (code.value == CODEPOINT_TAB) {

						_init_cbe_setg_select = Init_cbe_settings_select::PASSPHRASE_INPUT;
						update_dialog = true;
					}
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT ||
			    key == Input::KEY_ENTER) {

				if (_init_cbe_setg_passphrase.suitable() &&
				    _init_cbe_setg_size.is_nr_of_bytes_greater_than_zero() &&
				    _init_cbe_setg_select == Init_cbe_settings_select::START_BUTTON) {

					_init_cbe_setg_select = Init_cbe_settings_select::NONE;
					_state = State::INIT_CBE_DEVICE_CREATE_FILE;

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

					_state = State::INIT_CBE_DEVICE_INIT_CBE;
					update_sandbox_config = true;
					update_dialog = true;
				}
			}
		});
		break;

	case State::CBE_DEVICE_CONTROLS:

		event.handle_press([&] (Input::Keycode key, Codepoint code) {

			if (key == Input::BTN_LEFT) {

				Cbe_device_controls_selected const prev_select { _cbe_ctl_select };
				Cbe_device_controls_selected       next_select { Cbe_device_controls_selected::NONE };

				switch (_cbe_ctl_hover) {
				case Cbe_device_controls_hovered::REKEYING_START_BUTTON:

					next_select = Cbe_device_controls_selected::REKEYING_START_BUTTON;
					break;

				case Cbe_device_controls_hovered::RESIZING_START_BUTTON:

					next_select = Cbe_device_controls_selected::RESIZING_START_BUTTON;
					break;

				case Cbe_device_controls_hovered::RESIZING_NR_OF_BLKS_INPUT:

					next_select = Cbe_device_controls_selected::RESIZING_NR_OF_BLKS_INPUT;
					break;

				case Cbe_device_controls_hovered::CREATE_SNAPSHOT_BUTTON:

					next_select = Cbe_device_controls_selected::CREATE_SNAPSHOT_BUTTON;
					break;

				case Cbe_device_controls_hovered::DISCARD_SNAPSHOT_BUTTON:

					next_select = Cbe_device_controls_selected::DISCARD_SNAPSHOT_BUTTON;
					break;

				case Cbe_device_controls_hovered::NONE:

					next_select = Cbe_device_controls_selected::NONE;
					break;
				}
				if (_hovered_snap.valid() && _selected_snap != _hovered_snap) {

					_selected_snap = _hovered_snap;
					update_dialog = true;
				}
				if (next_select != prev_select) {

					_cbe_ctl_select = next_select;
					update_dialog = true;
				}

			} else {

				if (_cbe_ctl_select == Cbe_device_controls_selected::RESIZING_NR_OF_BLKS_INPUT) {

					if (codepoint_is_printable(code)) {

						_cbe_ctl_rsz_nr_of_blks.append_character(code);
						update_dialog = true;

					} else if (code.value == CODEPOINT_BACKSPACE) {

						_cbe_ctl_rsz_nr_of_blks.remove_last_character();
						update_dialog = true;
					}
				}
			}
		});
		event.handle_release([&] (Input::Keycode key) {

			if (key == Input::BTN_LEFT) {

				switch (_cbe_ctl_select) {
				case Cbe_device_controls_selected::RESIZING_START_BUTTON:

					_cbe_ctl_select = Cbe_device_controls_selected::NONE;
					_cbe_ctl_rsz_state = Cbe_device_controls_resizing_state::WAIT_TILL_DEVICE_IS_READY;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				case Cbe_device_controls_selected::REKEYING_START_BUTTON:

					_cbe_ctl_select = Cbe_device_controls_selected::NONE;
					_cbe_ctl_rky_state = Cbe_device_controls_rekeying_state::WAIT_TILL_DEVICE_IS_READY;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				case Cbe_device_controls_selected::CREATE_SNAPSHOT_BUTTON:

					_cbe_ctl_select = Cbe_device_controls_selected::NONE;
					_create_snap_state = Create_snapshot_state::ISSUE_REQUEST_AT_DEVICE;

					update_sandbox_config = true;
					update_dialog = true;
					break;

				case Cbe_device_controls_selected::DISCARD_SNAPSHOT_BUTTON:

					_cbe_ctl_select = Cbe_device_controls_selected::NONE;

					if (_selected_snap.valid()) {
						_discard_snap_state = Discard_snapshot_state::ISSUE_REQUEST_AT_DEVICE;
						_discard_snap_gen = _selected_snap.obj().generation();
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
/*
	log("--- hover ---");
	log(node);
	log("-------------");
*/
	bool update_dialog { false };

	switch (_state) {
	case State::INIT_TRUST_ANCHOR_SETTINGS:
	{
		Init_ta_settings_hover const prev_hover { _init_ta_setg_hover };
		Init_ta_settings_hover       next_hover { Init_ta_settings_hover::NONE };

		node.with_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_sub_node("vbox", [&] (Xml_node const &node_2) {

					node_2.with_sub_node("float", [&] (Xml_node const &node_3) {
						if (node_3.attribute_value("name", String<3>()) == "ok") {
							next_hover = Init_ta_settings_hover::START_BUTTON;
						}
					});

					node_2.with_sub_node("frame", [&] (Xml_node const &node_3) {
						if (node_3.attribute_value("name", String<4>()) == "pw") {
							next_hover = Init_ta_settings_hover::PASSPHRASE_INPUT;
						}
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_init_ta_setg_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::INIT_CBE_DEVICE_SETTINGS:
	{
		Init_cbe_settings_hover const prev_hover { _init_cbe_setg_hover };
		Init_cbe_settings_hover       next_hover { Init_cbe_settings_hover::NONE };

		node.with_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_sub_node("vbox", [&] (Xml_node const &node_2) {

					node_2.with_sub_node("float", [&] (Xml_node const &node_3) {
						if (node_3.attribute_value("name", String<8>()) == "ok") {
							next_hover = Init_cbe_settings_hover::START_BUTTON;
						}
					});

					node_2.with_sub_node("frame", [&] (Xml_node const &node_3) {
						if (node_3.attribute_value("name", String<8>()) == "pw") {
							next_hover = Init_cbe_settings_hover::PASSPHRASE_INPUT;
						} else if (node_3.attribute_value("name", String<4>()) == "sz") {
							next_hover = Init_cbe_settings_hover::SIZE_INPUT;
						}
					});
				});
			});
		});
		if (next_hover != prev_hover) {

			_init_cbe_setg_hover = next_hover;
			update_dialog = true;
		}
		break;
	}
	case State::CBE_DEVICE_CONTROLS:
	{
		Cbe_device_controls_hovered const prev_hover        { _cbe_ctl_hover };
		Cbe_device_controls_hovered       next_hover        { Cbe_device_controls_hovered::NONE };
		Const_pointer<Snapshot> const     prev_hovered_snap { _hovered_snap };
		Const_pointer<Snapshot>           next_hovered_snap { };

		node.with_sub_node("dialog", [&] (Xml_node const &node_0) {
			node_0.with_sub_node("frame", [&] (Xml_node const &node_1) {
				node_1.with_sub_node("vbox", [&] (Xml_node const &node_2) {

					node_2.with_sub_node("frame", [&] (Xml_node const &node_3) {
						node_3.with_sub_node("float", [&] (Xml_node const &node_4) {
							node_4.with_sub_node("vbox", [&] (Xml_node const &node_5) {
								node_5.with_sub_node("hbox", [&] (Xml_node const &node_6) {

									Generation const generation {
										node_6.attribute_value(
											"name", Generation { INVALID_GENERATION }) };

									if (generation != INVALID_GENERATION) {

										_snap_registry.for_each([&] (Snapshot const &snap)
										{
											if (generation == snap.generation()) {
												next_hovered_snap = snap;
											}
										});
									}
								});
							});
						});
					});

					node_2.with_sub_node("hbox", [&] (Xml_node const &node_3) {
						node_3.with_sub_node("frame", [&] (Xml_node const &node_4) {

							if (node_4.attribute_value("name", String<8>()) == "rky") {

								node_4.with_sub_node("vbox", [&] (Xml_node const &node_5) {
									node_5.with_sub_node("float", [&] (Xml_node const &node_6) {

										if (node_6.attribute_value("name", String<8>()) == "Start") {

											next_hover = Cbe_device_controls_hovered::REKEYING_START_BUTTON;

										} else if (node_6.attribute_value("name", String<16>()) == "Create snapshot") {

											next_hover = Cbe_device_controls_hovered::CREATE_SNAPSHOT_BUTTON;

										} else if (node_6.attribute_value("name", String<17>()) == "Discard snapshot") {

											next_hover = Cbe_device_controls_hovered::DISCARD_SNAPSHOT_BUTTON;
										}
									});
								});

							} else if (node_4.attribute_value("name", String<8>()) == "rsz") {

								node_4.with_sub_node("vbox", [&] (Xml_node const &node_5) {
									node_5.with_sub_node("float", [&] (Xml_node const &node_6) {

										if (node_6.attribute_value("name", String<8>()) == "Start") {
											next_hover = Cbe_device_controls_hovered::RESIZING_START_BUTTON;
										}
									});
									node_5.with_sub_node("frame", [&] (Xml_node const &node_6) {

										if (node_6.attribute_value("name", String<8>()) == "blks") {
											next_hover = Cbe_device_controls_hovered::RESIZING_NR_OF_BLKS_INPUT;
										}
									});
								});
							}
						});
					});
				});
			});
		});
		if (next_hovered_snap != prev_hovered_snap) {

			_hovered_snap = next_hovered_snap;
			update_dialog = true;
		}
		if (next_hover != prev_hover) {

			_cbe_ctl_hover = next_hover;
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
