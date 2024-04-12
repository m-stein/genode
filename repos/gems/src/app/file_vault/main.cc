/*
 * \brief  Graphical front end for controlling Tresor devices
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
#include <os/dynamic_rom_session.h>
#include <os/vfs.h>
#include <os/reporter.h>
#include <timer_session/connection.h>

/* tresor includes */
#include <tresor/types.h>

/* local includes */
#include <report_session_component.h>
#include <child_state.h>
#include <sandbox.h>
#include <input.h>
#include <utf8.h>
#include <child_exit_state.h>
#include <const_pointer.h>
#include <snapshot.h>
#include <capacity.h>

namespace File_vault {

	enum { PASSPHRASE_MIN_NR_OF_CHARS = 8 };

	class Ui_config;
	class Main;
}

struct File_vault::Ui_config
{
	using Version_string = String<80>;

	Version_string const version { };
	Passphrase const passphrase { };
	Number_of_bytes const client_fs_size { 0 };
	Number_of_bytes const journaling_buf_size { 0 };

	Ui_config() { }

	Ui_config(Xml_node const &node,
	          bool            verbose)
	:
		version             { node.attribute_value("version",             Version_string { }) },
		passphrase          { node.attribute_value("passphrase",          Passphrase { }) },
		client_fs_size      { node.attribute_value("client_fs_size",      Number_of_bytes { 0 }) },
		journaling_buf_size { node.attribute_value("journaling_buf_size", Number_of_bytes { 0 }) }
	{
		if (verbose)
			log("ui_config: version \"", version,
			    "\" passphrase ", passphrase_suitable() ? "<" : "<not ",
			    "suitable> client_fs_size ", client_fs_size,
			    " journaling_buf_size ", journaling_buf_size);
	}

	bool passphrase_suitable() const { return passphrase.length() >= PASSPHRASE_MIN_NR_OF_CHARS + 1; }
};

class File_vault::Main
:
	private Sandbox::Local_service_base::Wakeup,
	private Sandbox::State_handler
{
	private:

		enum Version { INVALID, VERSION_21_05, VERSION_23_05 };

		enum class State
		{
			INVALID,
			SETUP_OBTAIN_PARAMETERS,
			SETUP_CREATE_TRESOR_IMAGE_FILE,
			SETUP_RUN_TRESOR_INIT_TRUST_ANCHOR,
			SETUP_RUN_TRESOR_INIT,
			SETUP_START_TRESOR_VFS,
			SETUP_FORMAT_TRESOR,
			SETUP_DETERMINE_CLIENT_FS_SIZE,
			UNLOCK_OBTAIN_PARAMETERS,
			UNLOCK_RUN_TRESOR_INIT_TRUST_ANCHOR,
			UNLOCK_START_TRESOR_VFS,
			UNLOCK_DETERMINE_CLIENT_FS_SIZE,
			CONTROLS_ROOT,
			CONTROLS_SNAPSHOTS,
			CONTROLS_DIMENSIONS,
			CONTROLS_EXPAND_CLIENT_FS,
			CONTROLS_EXPAND_SNAPSHOT_BUF,
			CONTROLS_SECURITY,
			CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY,
			CONTROLS_SECURITY_MASTER_KEY,
			CONTROLS_SECURITY_USER_PASSPHRASE,
			LOCK_ISSUE_DEINIT_REQUEST_AT_TRESOR,
			LOCK_WAIT_TILL_DEINIT_REQUEST_IS_DONE
		};

		enum class Reported_state
		{
			INVALID,
			UNINITIALIZED,
			INITIALIZING,
			LOCKED,
			UNLOCKING,
			UNLOCKED,
			LOCKING
		};

		enum class Setup_obtain_params_hover
		{
			NONE,
			PASSPHRASE_INPUT,
			PASSPHRASE_SHOW_HIDE_BUTTON,
			CLIENT_FS_SIZE_INPUT,
			SNAPSHOT_BUFFER_SIZE_INPUT,
			START_BUTTON
		};

		enum class Setup_obtain_params_select
		{
			NONE,
			PASSPHRASE_INPUT,
			PASSPHRASE_SHOW_HIDE_BUTTON,
			CLIENT_FS_SIZE_INPUT,
			SNAPSHOT_BUFFER_SIZE_INPUT,
			START_BUTTON
		};

		enum class Controls_root_select
		{
			NONE,
			LOCK_BUTTON,
		};

		enum class Controls_root_hover
		{
			NONE,
			SNAPSHOTS_EXPAND_BUTTON,
			DIMENSIONS_BUTTON,
			SECURITY_EXPAND_BUTTON,
			LOCK_BUTTON,
		};

		enum class Controls_snapshots_select
		{
			NONE,
			LOCK_BUTTON,
			CREATE_BUTTON,
			GENERATION_DISCARD_BUTTON,
		};

		enum class Controls_snapshots_hover
		{
			NONE,
			LOCK_BUTTON,
			LEAVE_BUTTON,
			CREATE_BUTTON,
			GENERATION_LEAVE_BUTTON,
			GENERATION_DISCARD_BUTTON,
		};

		enum class Dimensions_select
		{
			NONE,
			EXPAND_CLIENT_FS_EXPAND_BUTTON,
			EXPAND_SNAP_BUF_EXPAND_BUTTON,
			LOCK_BUTTON,
		};

		enum class Dimensions_hover
		{
			NONE,
			LEAVE_BUTTON,
			EXPAND_CLIENT_FS_BUTTON,
			EXPAND_SNAPSHOT_BUF_BUTTON,
			LOCK_BUTTON,
		};

		enum class Expand_client_fs_select
		{
			NONE,
			CONTINGENT_INPUT,
			START_BUTTON,
			LOCK_BUTTON,
		};

		enum class Expand_client_fs_hover
		{
			NONE,
			LEAVE_BUTTON,
			CONTINGENT_INPUT,
			START_BUTTON,
			LOCK_BUTTON,
		};

		enum class Expand_snapshot_buf_select
		{
			NONE,
			CONTINGENT_INPUT,
			START_BUTTON,
			LOCK_BUTTON,
		};

		enum class Expand_snapshot_buf_hover
		{
			NONE,
			LEAVE_BUTTON,
			CONTINGENT_INPUT,
			START_BUTTON,
			LOCK_BUTTON,
		};

		enum class Controls_security_block_encryption_key_select
		{
			NONE,
			REPLACE_BUTTON,
			LOCK_BUTTON,
		};

		enum class Controls_security_block_encryption_key_hover
		{
			NONE,
			LEAVE_BUTTON,
			REPLACE_BUTTON,
			LOCK_BUTTON,
		};

		enum class Controls_security_master_key_select
		{
			NONE,
			LOCK_BUTTON,
		};

		enum class Controls_security_master_key_hover
		{
			NONE,
			LEAVE_BUTTON,
			LOCK_BUTTON,
		};

		enum class Controls_security_user_passphrase_select
		{
			NONE,
			LOCK_BUTTON,
		};

		enum class Controls_security_user_passphrase_hover
		{
			NONE,
			LEAVE_BUTTON,
			LOCK_BUTTON,
		};

		enum class Controls_security_select
		{
			NONE,
			BLOCK_ENCRYPTION_KEY_EXPAND_BUTTON,
			MASTER_KEY_EXPAND_BUTTON,
			USER_PASSPHRASE_EXPAND_BUTTON,
			LOCK_BUTTON,
		};

		enum class Controls_security_hover
		{
			NONE,
			SECURITY_EXPAND_BUTTON,
			BLOCK_ENCRYPTION_KEY_EXPAND_BUTTON,
			MASTER_KEY_EXPAND_BUTTON,
			USER_PASSPHRASE_EXPAND_BUTTON,
			LOCK_BUTTON,
		};

		enum class Resizing_type
		{
			NONE,
			EXPAND_CLIENT_FS,
			EXPAND_SNAPSHOT_BUF,
		};

		enum class Resizing_state
		{
			INACTIVE,
			ADAPT_TRESOR_IMAGE_SIZE,
			WAIT_TILL_DEVICE_IS_READY,
			ISSUE_REQUEST_AT_DEVICE,
			IN_PROGRESS_AT_DEVICE,
			DETERMINE_CLIENT_FS_SIZE,
			RUN_RESIZE2FS,
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
		using Xml_report_handler = Report::Session_component::Xml_handler<Main>;
		using State_string       = String<STATE_STRING_CAPACITY>;
		using Snapshot_registry  = Registry<Registered<Snapshot>>;
		using Snapshot_pointer   = Const_pointer<Snapshot>;

		Env                                   &_env;
		State                                  _state                              { State::INVALID };
		Heap                                   _heap                               { _env.ram(), _env.rm() };
		Timer::Connection                      _timer                              { _env };
		Attached_rom_dataspace                 _config_rom                         { _env, "config" };
		bool                                   _verbose_state                      { _config_rom.xml().attribute_value("verbose_state", false) };
		bool                                   _verbose_ui_config                  { _config_rom.xml().attribute_value("verbose_ui_config", false) };
		bool                                   _jent_avail                         { _config_rom.xml().attribute_value("jitterentropy_available", true) };
		Root_directory                         _vfs                                { _env, _heap, _config_rom.xml().sub_node("vfs") };
		Registry<Child_state>                  _children                           { };
		Child_state                            _mke2fs                             { _children, "mke2fs", Ram_quota { 100 * 1024 * 1024 }, Cap_quota { 500 } };
		Child_state                            _resize2fs                          { _children, "resize2fs", Ram_quota { 100 * 1024 * 1024 }, Cap_quota { 500 } };
		Child_state                            _tresor_vfs                            { _children, "tresor_vfs", "vfs", Ram_quota { 64 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _tresor_trust_anchor_vfs               { _children, "tresor_trust_anchor_vfs", "vfs", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _rump_vfs                           { _children, "rump_vfs", "vfs", Ram_quota { 32 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _sync_to_tresor_vfs_init               { _children, "sync_to_tresor_vfs_init", "file_vault-sync_to_tresor_vfs_init", Ram_quota { 8 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _truncate_file                      { _children, "truncate_file", "file_vault-truncate_file", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _tresor_vfs_block                      { _children, "vfs_block", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _fs_query                           { _children, "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _image_fs_query                     { _children, "image_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _client_fs_fs_query                 { _children, "client_fs_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _tresor_init_trust_anchor              { _children, "tresor_init_trust_anchor", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _tresor_init                           { _children, "tresor_init", Ram_quota { 4 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _snapshots_fs_query                 { _children, "snapshots_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _resizing_fs_tool                   { _children, "resizing_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _resizing_fs_query                  { _children, "resizing_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _rekeying_fs_tool                   { _children, "rekeying_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _rekeying_fs_query                  { _children, "rekeying_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _lock_fs_tool                       { _children, "lock_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _lock_fs_query                      { _children, "lock_fs_query", "fs_query", Ram_quota { 1 * 1024 * 1024 }, Cap_quota { 100 } };
		Child_state                            _create_snap_fs_tool                { _children, "create_snap_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Child_state                            _discard_snap_fs_tool               { _children, "discard_snap_fs_tool", "fs_tool", Ram_quota { 5 * 1024 * 1024 }, Cap_quota { 200 } };
		Xml_report_handler                     _fs_query_listing_handler           { *this, &Main::_handle_fs_query_listing };
		Xml_report_handler                     _image_fs_query_listing_handler     { *this, &Main::_handle_image_fs_query_listing };
		Xml_report_handler                     _client_fs_fs_query_listing_handler { *this, &Main::_handle_client_fs_fs_query_listing };
		Xml_report_handler                     _snapshots_fs_query_listing_handler { *this, &Main::_handle_snapshots_fs_query_listing };
		Xml_report_handler                     _resizing_fs_query_listing_handler  { *this, &Main::_handle_resizing_fs_query_listing };
		Xml_report_handler                     _rekeying_fs_query_listing_handler  { *this, &Main::_handle_rekeying_fs_query_listing };
		Xml_report_handler                     _lock_fs_query_listing_handler      { *this, &Main::_handle_lock_fs_query_listing };
		Sandbox                                _sandbox                            { _env, *this };
		Report_service                         _report_service                     { _sandbox, *this };
		Constructible<Watch_handler<Main>>     _watch_handler                      { };
		Constructible<Expanding_reporter>      _clipboard_reporter                 { };
		Constructible<Attached_rom_dataspace>  _clipboard_rom                      { };
		bool                                   _initial_config                     { true };
		Signal_handler<Main>                   _config_handler                     { _env.ep(), *this, &Main::_handle_config };
		Signal_handler<Main>                   _state_handler                      { _env.ep(), *this, &Main::_handle_state };
		Input_passphrase                       _setup_obtain_params_passphrase     { };
		Input_number_of_bytes                  _client_fs_size_input               { };
		Input_number_of_bytes                  _journaling_buf_size_input          { };
		Setup_obtain_params_hover              _setup_obtain_params_hover          { Setup_obtain_params_hover::NONE };
		Setup_obtain_params_select             _setup_obtain_params_select         { Setup_obtain_params_select::PASSPHRASE_INPUT };
		Controls_root_hover                    _controls_root_hover                { Controls_root_hover::NONE };
		Controls_root_select                   _controls_root_select               { Controls_root_select::NONE };
		Controls_snapshots_hover               _controls_snapshots_hover           { Controls_snapshots_hover::NONE };
		Controls_snapshots_select              _controls_snapshots_select          { Controls_snapshots_select::NONE };
		Dimensions_hover                       _dimensions_hover                   { Dimensions_hover::NONE };
		Dimensions_select                      _dimensions_select                  { Dimensions_select::NONE };
		Expand_client_fs_hover                 _expand_client_fs_hover             { Expand_client_fs_hover::NONE };
		Expand_client_fs_select                _expand_client_fs_select            { Expand_client_fs_select::NONE };
		Expand_snapshot_buf_hover              _expand_snapshot_buf_hover          { Expand_snapshot_buf_hover::NONE };
		Expand_snapshot_buf_select             _expand_snapshot_buf_select         { Expand_snapshot_buf_select::NONE };
		Controls_security_hover                _controls_security_hover            { Controls_security_hover::NONE };
		Controls_security_select               _controls_security_select           { Controls_security_select::NONE };

		Controls_security_master_key_hover             _controls_security_master_key_hover            { Controls_security_master_key_hover::NONE };
		Controls_security_master_key_select            _controls_security_master_key_select           { Controls_security_master_key_select::NONE };
		Controls_security_block_encryption_key_hover   _controls_security_block_encryption_key_hover  { Controls_security_block_encryption_key_hover::NONE };
		Controls_security_block_encryption_key_select  _controls_security_block_encryption_key_select { Controls_security_block_encryption_key_select::NONE };
		Controls_security_user_passphrase_hover        _controls_security_user_passphrase_hover       { Controls_security_user_passphrase_hover::NONE };
		Controls_security_user_passphrase_select       _controls_security_user_passphrase_select      { Controls_security_user_passphrase_select::NONE };

		Resizing_state                         _resizing_state                     { Resizing_state::INACTIVE };
		Resizing_type                          _resizing_type                      { Resizing_type::NONE };
		Input_number_of_bytes                  _expand_client_fs_contingent        { };
		Input_number_of_bytes                  _expand_snapshot_buf_contingent     { };
		Rekeying_state                         _rekeying_state                     { Rekeying_state::INACTIVE };
		Create_snapshot_state                  _create_snap_state                  { Create_snapshot_state::INACTIVE };
		Discard_snapshot_state                 _discard_snap_state                 { Discard_snapshot_state::INACTIVE };
		Generation                             _discard_snap_gen                   { INVALID_GENERATION };
		Snapshot_registry                      _snapshots                          { };
		Snapshot_pointer                       _snapshots_hover                    { };
		Snapshot_pointer                       _snapshots_select                   { };
		bool                                   _snapshots_expanded                 { false };
		bool                                   _dimensions_expanded                { false };
		Timer::One_shot_timeout<Main>          _unlock_retry_delay                 { _timer, *this, &Main::_handle_unlock_retry_delay };
		size_t                                 _tresor_image_size                  { 0 };
		File_path                              _tresor_image_file_name             { "tresor.img" };
		size_t                                 _client_fs_size                     { 0 };
		Number_of_clients                      _nr_of_clients                      { 0 };
		Constructible<Attached_rom_dataspace>  _ui_config_rom                      { };
		Signal_handler<Main>                   _ui_config_handler                  { _env.ep(), *this, &Main::_handle_ui_config };
		Constructible<Ui_config>               _ui_config                          { };
		Constructible<Expanding_reporter>      _ui_report                          { };
		Constructible<Rekey_config>            _rekey_config                       { };
		Constructible<Rekey_report>            _rekey_report                       { };
		Constructible<Extend_config>           _extend_config                      { };
		Constructible<Extend_report>           _extend_report                      { };

		static bool _has_name(Xml_node  const &node,
		                      Node_name const &name)
		{
			return node.attribute_value("name", Node_name { }) == name;
		}

		size_t _min_journaling_buf_size() const
		{
			size_t result { _ui_config->client_fs_size >> 8 };
			if (result < MIN_CLIENT_FS_SIZE) {
				result = MIN_CLIENT_FS_SIZE;
			}
			return result;
		}

		bool _ui_setup_obtain_params_passphrase_suitable() const
		{
			return _ui_config->passphrase.length() >= PASSPHRASE_MIN_NR_OF_CHARS + 1;
		}

		bool _ui_setup_obtain_params_suitable() const
		{
			return
				_ui_config->client_fs_size >= MIN_CLIENT_FS_SIZE &&
				_ui_config->journaling_buf_size >= _min_journaling_buf_size() &&
				_ui_setup_obtain_params_passphrase_suitable();
		}

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

		static size_t _child_nr_of_provided_sessions(Xml_node    const &sandbox_state,
		                                             Child_state const &child_state,
		                                             String<64>  const &service_name);

		void _handle_unlock_retry_delay(Duration);

		static State
		_state_and_version_from_string(State_string const &state_str,
		                               Version            &version);

		static State_string _state_to_string(State state);

		static State
		_state_and_version_from_fs_query_listing(Xml_node const &node,
		                                         Version        &version);

		void _write_to_state_file(State state);

		void _generate_sandbox_config(Xml_generator &xml) const;

		void _handle_fs_query_listing(Xml_node const &node);

		void _handle_image_fs_query_listing(Xml_node const &node);

		void _handle_client_fs_fs_query_listing(Xml_node const &node);

		void _handle_snapshots_fs_query_listing(Xml_node const &node);

		void _handle_resizing_fs_query_listing(Xml_node const &node);

		void _handle_rekeying_fs_query_listing(Xml_node const &node);

		void _handle_lock_fs_query_listing(Xml_node const &node);

		void _handle_config();

		void _handle_ui_config();

		void _handle_ui_config_and_report();

		void _handle_state();

		void _update_sandbox_config();

		void _adapt_to_version(Version version);

		Reported_state _reported_state() const;

		static char const *_reported_state_to_string(Reported_state state);

		void _generate_ui_report()
		{
			_ui_report->generate([&] (Xml_generator &xml) {
				xml.attribute("version", _ui_config->version);
				xml.attribute("state", _reported_state_to_string(_reported_state()));
				xml.attribute("image_size", _tresor_image_size);
				xml.attribute("capacity", _client_fs_size);
				xml.attribute("num_clients", _nr_of_clients.value);

				if (_rekey_report.constructed())
					xml.node("rekey", [&] { _rekey_report->generate(xml); });
				if (_extend_report.constructed())
					xml.node("extend", [&] { _extend_report->generate(xml); });
			});
		}

		void _set_state(State state)
		{
			Reported_state old_reported_state { _reported_state() };
			_state = state;
			Reported_state new_reported_state { _reported_state() };
			if (old_reported_state != new_reported_state)
				_generate_ui_report();
		}

		bool _rekey_operation_pending() const;

		bool _extend_operation_pending() const;

		static Number_of_blocks _tresor_tree_num_leaves(size_t payload_size);


		static size_t _tree_nr_of_blocks(size_t nr_of_lvls,
		                          size_t nr_of_children,
		                          size_t nr_of_leafs);

		size_t _tresor_size() const;

		static size_t _tresor_nr_of_blocks(size_t nr_of_superblocks,
		                                   size_t nr_of_vbd_lvls,
		                                   size_t nr_of_vbd_children,
		                                   size_t nr_of_vbd_leafs,
		                                   size_t nr_of_ft_lvls,
		                                   size_t nr_of_ft_children,
		                                   size_t nr_of_ft_leafs);

		template <size_t N>
		static bool listing_file_starts_with(Xml_node  const &fs_query_listing,
		                                     char      const *file_name,
		                                     String<N> const &str)
		{
			bool result { false };
			bool done   { false };
			fs_query_listing.with_optional_sub_node("dir", [&] (Xml_node const &node_0) {
				node_0.for_each_sub_node("file", [&] (Xml_node const &node_1) {
					if (done) {
						return;
					}
					if (node_1.attribute_value("name", String<16>()) == file_name) {
						node_1.with_raw_content([&] (char const *base, size_t size) {
							result = String<N> { Cstring {base, size} } == str;
							done = true;
						});
					}
				});
			});
			return result;
		}


		/***************************************************
		 ** Sandbox::Local_service_base::Wakeup interface **
		 ***************************************************/

		void wakeup_local_service() override;


		/****************************
		 ** Sandbox::State_handler **
		 ****************************/

		void handle_sandbox_state() override;


	public:

		Main(Env &env);
};

using namespace File_vault;


/**********************
 ** File_vault::Main **
 **********************/

void Main::_handle_config()
{
	_config_rom.update();
	_initial_config = false;
}


void Main::_handle_ui_config()
{
	_ui_config_rom->update();
	Xml_node const &ui_config = _ui_config_rom->xml();
	_ui_config.construct(ui_config, _verbose_ui_config);
	ui_config.with_optional_sub_node("rekey", [&] (Xml_node const &rekey) {
		_rekey_config.construct(rekey); });
	ui_config.with_optional_sub_node("extend", [&] (Xml_node const &extend) {
		_extend_config.construct(extend); });
	_handle_ui_config_and_report();
}


void Main::_update_sandbox_config()
{
	Buffered_xml const config { _heap, "config", [&] (Xml_generator &xml) {
		_generate_sandbox_config(xml); } };

	config.with_xml_node([&] (Xml_node const &config) {
		_sandbox.apply_config(config); });
}


void Main::_adapt_to_version(Version version)
{
	switch (version) {
	case VERSION_21_05:

		_tresor_image_file_name = "cbe.img";
		break;

	case VERSION_23_05:

		break;

	default:

		class Invalid_version { };
		throw Invalid_version { };
	}
}


Main::State Main::_state_and_version_from_string(State_string const &str,
                                                 Version            &version)
{
	version = VERSION_23_05;
	if (str == "invalid") { return State::INVALID; }
	if (str == "setup_obtain_parameters") { return State::SETUP_OBTAIN_PARAMETERS; }
	if (str == "setup_run_tresor_init_trust_anchor") { return State::SETUP_RUN_TRESOR_INIT_TRUST_ANCHOR; }
	if (str == "setup_create_tresor_image_file") { return State::SETUP_CREATE_TRESOR_IMAGE_FILE; }
	if (str == "setup_run_tresor_init") { return State::SETUP_RUN_TRESOR_INIT; }
	if (str == "setup_start_tresor_vfs") { return State::SETUP_START_TRESOR_VFS; }
	if (str == "setup_format_tresor") { return State::SETUP_FORMAT_TRESOR; }
	if (str == "setup_determine_client_fs_size") { return State::SETUP_DETERMINE_CLIENT_FS_SIZE; }
	if (str == "controls_root") { return State::CONTROLS_ROOT; }
	if (str == "controls_snapshots") { return State::CONTROLS_SNAPSHOTS; }
	if (str == "controls_dimensions") { return State::CONTROLS_DIMENSIONS; }
	if (str == "controls_expand_client_fs") { return State::CONTROLS_EXPAND_CLIENT_FS; }
	if (str == "controls_expand_snapshot_buf") { return State::CONTROLS_EXPAND_SNAPSHOT_BUF; }
	if (str == "controls_security") { return State::CONTROLS_SECURITY; }
	if (str == "controls_security_block_encryption_key") { return State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY; }
	if (str == "controls_security_master_key") { return State::CONTROLS_SECURITY_MASTER_KEY; }
	if (str == "controls_security_user_passphrase") { return State::CONTROLS_SECURITY_USER_PASSPHRASE; }
	if (str == "unlock_obtain_parameters") { return State::UNLOCK_OBTAIN_PARAMETERS; }
	if (str == "unlock_run_tresor_init_trust_anchor") { return State::UNLOCK_RUN_TRESOR_INIT_TRUST_ANCHOR; }
	if (str == "unlock_start_tresor_vfs") { return State::UNLOCK_START_TRESOR_VFS; }
	if (str == "unlock_determine_client_fs_size") { return State::UNLOCK_DETERMINE_CLIENT_FS_SIZE; }
	if (str == "lock_issue_deinit_request_at_tresor") { return State::LOCK_ISSUE_DEINIT_REQUEST_AT_TRESOR; }
	if (str == "lock_wait_till_deinit_request_is_done") { return State::LOCK_WAIT_TILL_DEINIT_REQUEST_IS_DONE; }

	version = VERSION_21_05;
	if (str == "startup_obtain_parameters") { return State::UNLOCK_OBTAIN_PARAMETERS; }

	class Invalid_state_string { };
	throw Invalid_state_string { };
}


Main::State_string Main::_state_to_string(State state)
{
	switch (state) {
	case State::INVALID:                                return "invalid";
	case State::SETUP_OBTAIN_PARAMETERS:                return "setup_obtain_parameters";
	case State::SETUP_RUN_TRESOR_INIT_TRUST_ANCHOR:     return "setup_run_tresor_init_trust_anchor";
	case State::SETUP_CREATE_TRESOR_IMAGE_FILE:         return "setup_create_tresor_image_file";
	case State::SETUP_RUN_TRESOR_INIT:                  return "setup_run_tresor_init";
	case State::SETUP_START_TRESOR_VFS:                 return "setup_start_tresor_vfs";
	case State::SETUP_FORMAT_TRESOR:                    return "setup_format_tresor";
	case State::SETUP_DETERMINE_CLIENT_FS_SIZE:         return "setup_determine_client_fs_size";
	case State::CONTROLS_ROOT:                          return "controls_root";
	case State::CONTROLS_SNAPSHOTS:                     return "controls_snapshots";
	case State::CONTROLS_DIMENSIONS:                    return "controls_dimensions";
	case State::CONTROLS_EXPAND_CLIENT_FS:              return "controls_expand_client_fs";
	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:           return "controls_expand_snapshot_buf";
	case State::CONTROLS_SECURITY:                      return "controls_security";
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY: return "controls_security_block_encryption_key";
	case State::CONTROLS_SECURITY_MASTER_KEY:           return "controls_security_master_key";
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:      return "controls_security_user_passphrase";
	case State::UNLOCK_OBTAIN_PARAMETERS:               return "unlock_obtain_parameters";
	case State::UNLOCK_RUN_TRESOR_INIT_TRUST_ANCHOR:    return "unlock_run_tresor_init_trust_anchor";
	case State::UNLOCK_START_TRESOR_VFS:                return "unlock_start_tresor_vfs";
	case State::UNLOCK_DETERMINE_CLIENT_FS_SIZE:        return "unlock_determine_client_fs_size";
	case State::LOCK_ISSUE_DEINIT_REQUEST_AT_TRESOR:    return "lock_issue_deinit_request_at_tresor";
	case State::LOCK_WAIT_TILL_DEINIT_REQUEST_IS_DONE:  return "lock_wait_till_deinit_request_is_done";
	}
	class Invalid_state { };
	throw Invalid_state { };
}


char const *Main::_reported_state_to_string(Reported_state state)
{
	switch (state) {
	case Reported_state::INVALID: return "invalid";
	case Reported_state::UNINITIALIZED: return "uninitialized";
	case Reported_state::INITIALIZING: return "initializing";
	case Reported_state::LOCKED: return "locked";
	case Reported_state::UNLOCKING: return "unlocking";
	case Reported_state::UNLOCKED: return "unlocked";
	case Reported_state::LOCKING: return "locking";
	}
	class Invalid_state { };
	throw Invalid_state { };
}


Main::Reported_state Main::_reported_state() const
{
	switch (_state) {
	case State::INVALID:                                return Reported_state::INVALID;
	case State::SETUP_OBTAIN_PARAMETERS:                return Reported_state::UNINITIALIZED;
	case State::SETUP_CREATE_TRESOR_IMAGE_FILE:         return Reported_state::INITIALIZING;
	case State::SETUP_RUN_TRESOR_INIT_TRUST_ANCHOR:     return Reported_state::INITIALIZING;
	case State::SETUP_RUN_TRESOR_INIT:                  return Reported_state::INITIALIZING;
	case State::SETUP_START_TRESOR_VFS:                 return Reported_state::INITIALIZING;
	case State::SETUP_FORMAT_TRESOR:                    return Reported_state::INITIALIZING;
	case State::SETUP_DETERMINE_CLIENT_FS_SIZE:         return Reported_state::INITIALIZING;
	case State::CONTROLS_ROOT:                          return Reported_state::UNLOCKED;
	case State::CONTROLS_SNAPSHOTS:                     return Reported_state::UNLOCKED;
	case State::CONTROLS_DIMENSIONS:                    return Reported_state::UNLOCKED;
	case State::CONTROLS_EXPAND_CLIENT_FS:              return Reported_state::UNLOCKED;
	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:           return Reported_state::UNLOCKED;
	case State::CONTROLS_SECURITY:                      return Reported_state::UNLOCKED;
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY: return Reported_state::UNLOCKED;
	case State::CONTROLS_SECURITY_MASTER_KEY:           return Reported_state::UNLOCKED;
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:      return Reported_state::UNLOCKED;
	case State::UNLOCK_OBTAIN_PARAMETERS:               return Reported_state::LOCKED;
	case State::UNLOCK_RUN_TRESOR_INIT_TRUST_ANCHOR:    return Reported_state::UNLOCKING;
	case State::UNLOCK_START_TRESOR_VFS:                return Reported_state::UNLOCKING;
	case State::UNLOCK_DETERMINE_CLIENT_FS_SIZE:        return Reported_state::UNLOCKING;
	case State::LOCK_ISSUE_DEINIT_REQUEST_AT_TRESOR:    return Reported_state::LOCKING;
	case State::LOCK_WAIT_TILL_DEINIT_REQUEST_IS_DONE:  return Reported_state::LOCKING;
	}
	class Invalid_state { };
	throw Invalid_state { };
}


Main::State
Main::_state_and_version_from_fs_query_listing(Xml_node const &node,
                                               Version        &version)
{
	State state { State::INVALID };
	bool state_file_found { false };
	node.with_optional_sub_node("dir", [&] (Xml_node const &node_0) {
		node_0.with_optional_sub_node("file", [&] (Xml_node const &node_1) {
			if (_has_name(node_1, "state")) {
				state_file_found = true;
				state =_state_and_version_from_string(
					node_1.decoded_content<State_string>(), version);
				
			}
		});
	});
	if (!state_file_found)
		version = VERSION_23_05;

	return state;
}


void Main::_write_to_state_file(State state)
{
	bool write_error = false;
	try {
		New_file new_file(_vfs, Directory::Path("/tresor/file_vault/state"));
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


void Main::_handle_resizing_fs_query_listing(Xml_node const &node)
{
	switch (_state) {
	case State::CONTROLS_ROOT:
	case State::CONTROLS_SNAPSHOTS:
	case State::CONTROLS_DIMENSIONS:
	case State::CONTROLS_EXPAND_CLIENT_FS:
	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:
	case State::CONTROLS_SECURITY:
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	case State::CONTROLS_SECURITY_MASTER_KEY:
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:

		switch (_resizing_state) {
		case Resizing_state::WAIT_TILL_DEVICE_IS_READY:

			if (listing_file_starts_with(node, "extend", String<10>("succeeded")) ||
			    listing_file_starts_with(node, "extend", String<5>("none"))) {

				_resizing_state = Resizing_state::ISSUE_REQUEST_AT_DEVICE;
				Signal_transmitter(_state_handler).submit();

			} else
				error("failed to extend: tresor not ready");

			break;

		case Resizing_state::IN_PROGRESS_AT_DEVICE:

			if (listing_file_starts_with(node, "extend", String<10>("succeeded"))) {

				switch (_resizing_type) {
				case Resizing_type::EXPAND_CLIENT_FS:

					_expand_client_fs_contingent = Input_number_of_bytes { };
					_expand_client_fs_select = Expand_client_fs_select::CONTINGENT_INPUT;
					break;

				case Resizing_type::EXPAND_SNAPSHOT_BUF:

					_expand_snapshot_buf_contingent = Input_number_of_bytes { };
					_expand_snapshot_buf_select = Expand_snapshot_buf_select::CONTINGENT_INPUT;
					break;

				default:

					class Unexpected_resizing_type { };
					throw Unexpected_resizing_type { };
					break;
				}
				_resizing_state = Resizing_state::DETERMINE_CLIENT_FS_SIZE;
				Signal_transmitter(_state_handler).submit();

			} else
				error("failed to extend: operation failed at tresor");

			break;

		default:

			break;
		}

	default:

		break;
	}
}


void Main::_handle_lock_fs_query_listing(Xml_node const &node)
{
	switch (_state) {
	case State::LOCK_WAIT_TILL_DEINIT_REQUEST_IS_DONE:

		if (listing_file_starts_with(node, "deinitialize", String<10>("succeeded"))) {

			_set_state(State::UNLOCK_OBTAIN_PARAMETERS);
			_setup_obtain_params_passphrase = Input_passphrase { };
			_setup_obtain_params_select = Setup_obtain_params_select::PASSPHRASE_INPUT;
			Signal_transmitter(_state_handler).submit();

		} else
			error("failed to deinitialize: operation failed at tresor");

		break;

	default:

		break;
	}
}


void Main::_handle_rekeying_fs_query_listing(Xml_node const &node)
{
	bool generate_ui_report = false;
	switch (_state) {
	case State::CONTROLS_ROOT:
	case State::CONTROLS_SNAPSHOTS:
	case State::CONTROLS_DIMENSIONS:
	case State::CONTROLS_EXPAND_CLIENT_FS:
	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:
	case State::CONTROLS_SECURITY:
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	case State::CONTROLS_SECURITY_MASTER_KEY:
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:

		switch (_rekeying_state) {
		case Rekeying_state::WAIT_TILL_DEVICE_IS_READY:

			if (listing_file_starts_with(node, "rekey", String<10>("succeeded")) ||
			    listing_file_starts_with(node, "rekey", String<5>("none"))) {

				_rekeying_state = Rekeying_state::ISSUE_REQUEST_AT_DEVICE;
				Signal_transmitter(_state_handler).submit();

			} else
				error("failed to rekey: tresor not ready");

			break;

		case Rekeying_state::IN_PROGRESS_AT_DEVICE:

			if (listing_file_starts_with(node, "rekey", String<10>("succeeded"))) {

log("rekeying finished");

				_rekey_report->finished = true;
				generate_ui_report = true;
				_rekeying_state = Rekeying_state::INACTIVE;
				Signal_transmitter(_state_handler).submit();

			} else
				error("failed to rekey: operation failed at tresor");

			break;

		default:

			break;
		}
		break;

	default:

		break;
	}
	if (generate_ui_report)
		_generate_ui_report();
}


void Main::_handle_snapshots_fs_query_listing(Xml_node const &node)
{
	switch (_state) {
	case State::CONTROLS_ROOT:
	case State::CONTROLS_SNAPSHOTS:
	case State::CONTROLS_DIMENSIONS:
	case State::CONTROLS_EXPAND_CLIENT_FS:
	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:
	case State::CONTROLS_SECURITY:
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	case State::CONTROLS_SECURITY_MASTER_KEY:
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:
	{
		node.with_optional_sub_node("dir", [&] (Xml_node const &node_0) {

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
				}
			});
		});
		break;
	}
	default:

		break;
	}
}


void Main::_handle_fs_query_listing(Xml_node const &node)
{
	switch (_state) {
	case State::INVALID:
	{
		Version version { INVALID };
		State const state { _state_and_version_from_fs_query_listing(node, version) };
		_adapt_to_version(version);

		switch (state) {
		case State::INVALID:

			_set_state(State::SETUP_OBTAIN_PARAMETERS);
			Signal_transmitter(_state_handler).submit();
			break;

		case State::UNLOCK_OBTAIN_PARAMETERS:

			_set_state(State::UNLOCK_OBTAIN_PARAMETERS);
			Signal_transmitter(_state_handler).submit();
			break;

		default:

			class Unexpected_state { };
			throw Unexpected_state { };
		}
		break;
	}
	default:

		break;
	}
}


void Main::_handle_client_fs_fs_query_listing(Xml_node const &node)
{
	bool generate_ui_report = false;
	switch (_state) {
	case State::SETUP_DETERMINE_CLIENT_FS_SIZE:
	case State::UNLOCK_DETERMINE_CLIENT_FS_SIZE:

		node.with_optional_sub_node("dir", [&] (Xml_node const &node_0) {
			node_0.with_optional_sub_node("file", [&] (Xml_node const &node_1) {

				if (_has_name(node_1, "data")) {

					_client_fs_size = node_1.attribute_value("size", (size_t)0);
					generate_ui_report = true;
					_set_state(State::CONTROLS_ROOT);
					Signal_transmitter(_state_handler).submit();
				}
			});
		});
		break;

	case State::CONTROLS_ROOT:
	case State::CONTROLS_SNAPSHOTS:
	case State::CONTROLS_DIMENSIONS:
	case State::CONTROLS_EXPAND_CLIENT_FS:
	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:
	case State::CONTROLS_SECURITY:
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	case State::CONTROLS_SECURITY_MASTER_KEY:
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:

		switch (_resizing_state) {
		case Resizing_state::DETERMINE_CLIENT_FS_SIZE:

			node.with_optional_sub_node("dir", [&] (Xml_node const &node_0) {
				node_0.with_optional_sub_node("file", [&] (Xml_node const &node_1) {

					if (_has_name(node_1, "data")) {

						size_t const size {
							node_1.attribute_value("size", (size_t)0) };

						if (_client_fs_size != size) {

							_client_fs_size = size;
							generate_ui_report = true;
							_resizing_state = Resizing_state::RUN_RESIZE2FS;
							Signal_transmitter(_state_handler).submit();

						} else {

							_resizing_type = Resizing_type::NONE;
							_resizing_state = Resizing_state::INACTIVE;
							_extend_report->finished = true;
							generate_ui_report = true;
							Signal_transmitter(_state_handler).submit();
						}
					}
				});
			});
			break;

		default:

			break;
		}

	default:

		break;
	}
	if (generate_ui_report)
		_generate_ui_report();
}


void Main::_handle_image_fs_query_listing(Xml_node const &node)
{
	bool generate_ui_report { false };

	switch (_state) {
	case State::CONTROLS_ROOT:
	case State::CONTROLS_SNAPSHOTS:
	case State::CONTROLS_DIMENSIONS:
	case State::CONTROLS_EXPAND_CLIENT_FS:
	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:
	case State::CONTROLS_SECURITY:
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	case State::CONTROLS_SECURITY_MASTER_KEY:
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:
	{
		size_t size { 0 };
		node.with_optional_sub_node("dir", [&] (Xml_node const &node_0) {
			node_0.with_optional_sub_node("file", [&] (Xml_node const &node_1) {
				if (_has_name(node_1, _tresor_image_file_name)) {
					size = node_1.attribute_value("size", (size_t)0);
				}
			});
		});
		if (_tresor_image_size != size) {

			_tresor_image_size = size;
			generate_ui_report = true;
		}
		break;
	}
	default:

		break;
	}
	if (generate_ui_report)
		_generate_ui_report();
}


void Main::_handle_state()
{
	_update_sandbox_config();
	_handle_ui_config_and_report();
}


bool Main::_rekey_operation_pending() const
{
	if (!_rekey_config.constructed())
		return false;

	if (!_rekey_report.constructed())
		return true;

	return _rekey_report->id.value != _rekey_config->id.value;
}


bool Main::_extend_operation_pending() const
{
	if (!_extend_config.constructed())
		return false;

	if (!_extend_report.constructed())
		return true;

	return _extend_report->id.value != _extend_config->id.value;
}


void Main::_handle_ui_config_and_report()
{
	bool update_sandbox_config { false };

	switch (_state) {
	case State::SETUP_OBTAIN_PARAMETERS:

		if(_ui_setup_obtain_params_suitable()) {

			_set_state(State::SETUP_CREATE_TRESOR_IMAGE_FILE);
			update_sandbox_config = true;
		}
		break;

	case State::UNLOCK_OBTAIN_PARAMETERS:

		if (_ui_setup_obtain_params_passphrase_suitable()) {

			_set_state(State::UNLOCK_RUN_TRESOR_INIT_TRUST_ANCHOR);
			update_sandbox_config = true;
		}
		break;

	case State::CONTROLS_ROOT:
	case State::CONTROLS_SNAPSHOTS:
	case State::CONTROLS_DIMENSIONS:
	case State::CONTROLS_EXPAND_CLIENT_FS:
	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:
	case State::CONTROLS_SECURITY:
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	case State::CONTROLS_SECURITY_MASTER_KEY:
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:

		if (!_ui_setup_obtain_params_passphrase_suitable()) {

			_set_state(State::LOCK_ISSUE_DEINIT_REQUEST_AT_TRESOR);
			update_sandbox_config = true;
			break;
		}
		if (_rekeying_state == Rekeying_state::INACTIVE && _rekey_operation_pending()) {

log("rekeying started");

			_rekey_report.construct(_rekey_config->id, false);
			_rekeying_state = Rekeying_state::WAIT_TILL_DEVICE_IS_READY;
			_update_sandbox_config();
			_generate_ui_report();
		}
		if (_resizing_state == Resizing_state::INACTIVE && _extend_operation_pending()) {

log("resizing started");

			_extend_report.construct(_extend_config->id, false);
			_resizing_state = Resizing_state::ADAPT_TRESOR_IMAGE_SIZE;
			switch (_extend_config->tree) {
			case Extend_config::VIRTUAL_BLOCK_DEVICE: _resizing_type = Resizing_type::EXPAND_CLIENT_FS; break;
			case Extend_config::FREE_TREE: _resizing_type = Resizing_type::EXPAND_SNAPSHOT_BUF; break;
			}
			_update_sandbox_config();
			_generate_ui_report();
		}
		break;

	default: break;
	}
	if (update_sandbox_config)
		_update_sandbox_config();
}


Main::Main(Env &env) : _env(env)
{
	_config_rom.sigh(_config_handler);
	_handle_config();
	_update_sandbox_config();
	_ui_config_rom.construct(_env, "ui_config");
	_ui_config_rom->sigh(_ui_config_handler);
	_ui_report.construct(_env, "ui_report", "ui_report");
	_handle_ui_config();
	_set_state(State::INVALID);
}


size_t
File_vault::Main::
_child_nr_of_provided_sessions(Xml_node    const &sandbox_state,
                               Child_state const &child_state,
                               String<64>  const &service_name)
{
	size_t result { 0 };
	sandbox_state.for_each_sub_node("child", [&] (Xml_node child) {

		if (child.attribute_value("name", String<128> { }) == child_state.start_name()) {

			child.with_optional_sub_node("provided", [&] (Xml_node const &provided) {
				provided.for_each_sub_node("session", [&] (Xml_node const &session) {

					if (session.attribute_value("service", String<64> { }) == service_name) {
						result++;
					}
				});
			});
		}
	});
	return result;
}


bool File_vault::Main::_child_succeeded(Xml_node    const &sandbox_state,
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

void File_vault::Main::_handle_unlock_retry_delay(Duration)
{
	_set_state(State::UNLOCK_OBTAIN_PARAMETERS);
	_ui_config.construct();
	_setup_obtain_params_passphrase = Input_passphrase { };
	_setup_obtain_params_select = Setup_obtain_params_select::PASSPHRASE_INPUT;
	Signal_transmitter(_state_handler).submit();
}


void File_vault::Main::handle_sandbox_state()
{
	Buffered_xml sandbox_state {
		_heap, "sandbox_state",
		[&] (Xml_generator &xml) {
			_sandbox.generate_state_report(xml);
		}
	};
	bool update_sandbox { false };
	bool generate_ui_report { false };
	Number_of_clients nr_of_clients { 0 };
	sandbox_state.with_xml_node([&] (Xml_node const &sandbox_state) {

		switch (_state) {
		case State::SETUP_RUN_TRESOR_INIT_TRUST_ANCHOR:

			if (_child_succeeded(sandbox_state, _tresor_init_trust_anchor)) {

				_set_state(State::SETUP_RUN_TRESOR_INIT);
				update_sandbox = true;
			}
			break;

		case State::SETUP_CREATE_TRESOR_IMAGE_FILE:

			if (_child_succeeded(sandbox_state, _truncate_file)) {

				_set_state(State::SETUP_RUN_TRESOR_INIT_TRUST_ANCHOR);
				update_sandbox = true;
			}
			break;

		case State::UNLOCK_RUN_TRESOR_INIT_TRUST_ANCHOR:

			_if_child_exited(sandbox_state, _tresor_init_trust_anchor, [&] (int exit_code) {

				if (exit_code == 0) {

					_set_state(State::UNLOCK_START_TRESOR_VFS);
					update_sandbox = true;

				} else
					_unlock_retry_delay.schedule(Microseconds { 3000000 });
			});
			break;

		case State::SETUP_RUN_TRESOR_INIT:

			if (_child_succeeded(sandbox_state, _tresor_init)) {

				_set_state(State::SETUP_START_TRESOR_VFS);
				update_sandbox = true;
			}
			break;

		case State::SETUP_START_TRESOR_VFS:

			if (_child_succeeded(sandbox_state, _sync_to_tresor_vfs_init)) {

				_set_state(State::SETUP_FORMAT_TRESOR);
				update_sandbox = true;
			}
			break;

		case State::UNLOCK_START_TRESOR_VFS:

			if (_child_succeeded(sandbox_state, _sync_to_tresor_vfs_init)) {

				_set_state(State::UNLOCK_DETERMINE_CLIENT_FS_SIZE);
				update_sandbox = true;
			}
			break;

		case State::SETUP_FORMAT_TRESOR:

			if (_child_succeeded(sandbox_state, _mke2fs)) {

				_write_to_state_file(State::UNLOCK_OBTAIN_PARAMETERS);
				_set_state(State::SETUP_DETERMINE_CLIENT_FS_SIZE);
				update_sandbox = true;
			}
			break;

		case State::CONTROLS_ROOT:
		case State::CONTROLS_SNAPSHOTS:
		case State::CONTROLS_DIMENSIONS:
		case State::CONTROLS_EXPAND_CLIENT_FS:
		case State::CONTROLS_EXPAND_SNAPSHOT_BUF:
		case State::CONTROLS_SECURITY:
		case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
		case State::CONTROLS_SECURITY_MASTER_KEY:
		case State::CONTROLS_SECURITY_USER_PASSPHRASE:

			if (_resizing_state == Resizing_state::INACTIVE ||
			    _resizing_type != Resizing_type::EXPAND_CLIENT_FS)
			{
				nr_of_clients.value =
					_child_nr_of_provided_sessions(
						sandbox_state, _rump_vfs, "File_system");
			}
			switch (_resizing_state) {
			case Resizing_state::ADAPT_TRESOR_IMAGE_SIZE:

				if (_child_succeeded(sandbox_state, _truncate_file)) {

					_resizing_state = Resizing_state::WAIT_TILL_DEVICE_IS_READY;
					update_sandbox = true;
				}
				break;

			case Resizing_state::ISSUE_REQUEST_AT_DEVICE:

				if (_child_succeeded(sandbox_state, _resizing_fs_tool)) {

					_resizing_state = Resizing_state::IN_PROGRESS_AT_DEVICE;
					update_sandbox = true;
				}
				break;

			case Resizing_state::RUN_RESIZE2FS:

				if (_child_succeeded(sandbox_state, _resize2fs)) {

					_resizing_type = Resizing_type::NONE;
					_resizing_state = Resizing_state::INACTIVE;
					_extend_report->finished = true;
					generate_ui_report = true;
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
					update_sandbox = true;
				}
				break;

			default:

				break;
			}

			break;

		case State::LOCK_ISSUE_DEINIT_REQUEST_AT_TRESOR:

			if (_child_succeeded(sandbox_state, _lock_fs_tool)) {

				if (_rekey_report.constructed()) {
					_rekey_report->finished = true;
					_rekeying_state = Rekeying_state::INACTIVE;
					generate_ui_report = true;
				}
				if (_extend_report.constructed()) {
					_extend_report->finished = true;
					_resizing_type = Resizing_type::NONE;
					_resizing_state = Resizing_state::INACTIVE;
					generate_ui_report = true;
				}
				_set_state(State::LOCK_WAIT_TILL_DEINIT_REQUEST_IS_DONE);
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
	if (_nr_of_clients.value != nr_of_clients.value) {
		_nr_of_clients.value = nr_of_clients.value;
		generate_ui_report = true;
	}
	if (update_sandbox)
		_update_sandbox_config();

	if (generate_ui_report)
		_generate_ui_report();
}


void File_vault::Main::wakeup_local_service()
{
	_report_service.for_each_requested_session([&] (Report_service::Request &request) {

		if (request.label == "fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _fs_query_listing_handler, _env.ep(),
					request.resources, "", request.diag) };

			request.deliver_session(session);

		} if (request.label == "image_fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _image_fs_query_listing_handler, _env.ep(),
					request.resources, "", request.diag) };

			request.deliver_session(session);

		} else if (request.label == "client_fs_fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _client_fs_fs_query_listing_handler, _env.ep(),
					request.resources, "", request.diag) };

			request.deliver_session(session);

		} else if (request.label == "snapshots_fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _snapshots_fs_query_listing_handler, _env.ep(),
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

		} else if (request.label == "lock_fs_query -> listing") {

			Report::Session_component &session { *new (_heap)
				Report::Session_component(
					_env, _lock_fs_query_listing_handler, _env.ep(),
					request.resources, "", request.diag) };

			request.deliver_session(session);

		} else {

			error("failed to deliver Report session with label ", request.label);
		}
	});

	_report_service.for_each_session_to_close([&] (Report::Session_component &session) {

		destroy(_heap, &session);
		return Report_service::Close_response::CLOSED;
	});
}


Number_of_blocks Main::_tresor_tree_num_leaves(size_t payload_size)
{
	Number_of_blocks nr_of_leaves { payload_size / TRESOR_BLOCK_SIZE };
	if (payload_size % TRESOR_BLOCK_SIZE) {
		nr_of_leaves++;
	}
	return nr_of_leaves;
}


void File_vault::Main::_generate_sandbox_config(Xml_generator &xml) const
{
	switch (_state) {
	case State::INVALID:

		gen_parent_provides_and_report_nodes(xml);
		gen_fs_query_start_node(xml, _fs_query);
		break;

	case State::SETUP_OBTAIN_PARAMETERS:

		gen_parent_provides_and_report_nodes(xml);
		break;

	case State::UNLOCK_OBTAIN_PARAMETERS:

		gen_parent_provides_and_report_nodes(xml);
		break;

	case State::SETUP_RUN_TRESOR_INIT_TRUST_ANCHOR:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_init_trust_anchor_start_node(
			xml, _tresor_init_trust_anchor, _ui_config->passphrase);

		break;

	case State::UNLOCK_RUN_TRESOR_INIT_TRUST_ANCHOR:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_init_trust_anchor_start_node(
			xml, _tresor_init_trust_anchor, _ui_config->passphrase);

		break;

	case State::UNLOCK_START_TRESOR_VFS:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_vfs_start_node(xml, _tresor_vfs, _tresor_image_file_name);
		gen_sync_to_tresor_vfs_init_start_node(xml, _sync_to_tresor_vfs_init);
		break;

	case State::SETUP_DETERMINE_CLIENT_FS_SIZE:
	case State::UNLOCK_DETERMINE_CLIENT_FS_SIZE:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_vfs_start_node(xml, _tresor_vfs, _tresor_image_file_name);
		gen_client_fs_fs_query_start_node(xml, _client_fs_fs_query);
		break;

	case State::SETUP_CREATE_TRESOR_IMAGE_FILE:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_truncate_file_start_node(
			xml, _truncate_file,
			File_path { "/tresor/", _tresor_image_file_name }.string(),
			TRESOR_BLOCK_SIZE *
				_tresor_nr_of_blocks(
					TRESOR_NR_OF_SUPERBLOCKS,
					TRESOR_VBD_MAX_LVL + 1,
					TRESOR_VBD_DEGREE,
					_tresor_tree_num_leaves(_ui_config->client_fs_size),
					TRESOR_FREE_TREE_MAX_LVL + 1,
					TRESOR_FREE_TREE_DEGREE,
					_tresor_tree_num_leaves(_ui_config->journaling_buf_size)));

		break;

	case State::SETUP_RUN_TRESOR_INIT:
	{
		Tresor::Superblock_configuration sb_config {
			Tree_configuration { TRESOR_VBD_MAX_LVL, TRESOR_VBD_DEGREE, _tresor_tree_num_leaves(_ui_config->client_fs_size) },
			Tree_configuration { TRESOR_FREE_TREE_MAX_LVL, TRESOR_FREE_TREE_DEGREE, _tresor_tree_num_leaves(_ui_config->journaling_buf_size) }
		};
		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_init_start_node(xml, _tresor_init, sb_config);
		break;
	}
	case State::SETUP_START_TRESOR_VFS:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_vfs_start_node(xml, _tresor_vfs, _tresor_image_file_name);
		gen_sync_to_tresor_vfs_init_start_node(xml, _sync_to_tresor_vfs_init);
		break;

	case State::SETUP_FORMAT_TRESOR:

		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_vfs_start_node(xml, _tresor_vfs, _tresor_image_file_name);
		gen_tresor_vfs_block_start_node(xml, _tresor_vfs_block);
		gen_mke2fs_start_node(xml, _mke2fs);
		break;

	case State::CONTROLS_ROOT:
	case State::CONTROLS_SNAPSHOTS:
	case State::CONTROLS_DIMENSIONS:
	case State::CONTROLS_EXPAND_CLIENT_FS:
	case State::CONTROLS_EXPAND_SNAPSHOT_BUF:
	case State::CONTROLS_SECURITY:
	case State::CONTROLS_SECURITY_BLOCK_ENCRYPTION_KEY:
	case State::CONTROLS_SECURITY_MASTER_KEY:
	case State::CONTROLS_SECURITY_USER_PASSPHRASE:
	{
		gen_parent_provides_and_report_nodes(xml);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_vfs_start_node(xml, _tresor_vfs, _tresor_image_file_name);
		gen_tresor_vfs_block_start_node(xml, _tresor_vfs_block);
		gen_snapshots_fs_query_start_node(xml, _snapshots_fs_query);
		gen_image_fs_query_start_node(xml, _image_fs_query);

		switch(_resizing_state) {
		case Resizing_state::INACTIVE:

			break;

		case Resizing_state::ADAPT_TRESOR_IMAGE_SIZE:

			switch (_resizing_type) {
			case Resizing_type::EXPAND_CLIENT_FS:
			{
				size_t const bytes {
					_extend_config->num_bytes };

				size_t const effective_bytes {
					bytes - (bytes % TRESOR_BLOCK_SIZE) };

				gen_truncate_file_start_node(
					xml, _truncate_file,
					File_path { "/tresor/", _tresor_image_file_name }.string(),
					_tresor_image_size + effective_bytes);

				break;
			}
			case Resizing_type::EXPAND_SNAPSHOT_BUF:
			{
				size_t const bytes {
					_extend_config->num_bytes };

				size_t const effective_bytes {
					bytes - (bytes % TRESOR_BLOCK_SIZE) };

				gen_truncate_file_start_node(
					xml, _truncate_file,
					File_path { "/tresor/", _tresor_image_file_name }.string(),
					_tresor_image_size + effective_bytes);

				break;
			}
			default:

				class Unexpected_resizing_type { };
				throw Unexpected_resizing_type { };
				break;
			}
			break;

		case Resizing_state::WAIT_TILL_DEVICE_IS_READY:

			gen_resizing_fs_query_start_node(xml, _resizing_fs_query);
			break;

		case Resizing_state::ISSUE_REQUEST_AT_DEVICE:

			switch (_resizing_type) {
			case Resizing_type::EXPAND_CLIENT_FS:

				gen_resizing_fs_tool_start_node(
					xml, _resizing_fs_tool, "vbd",
					_extend_config->num_bytes / TRESOR_BLOCK_SIZE);

log("resizing in progress num_blocks=", _extend_config->num_bytes / TRESOR_BLOCK_SIZE);

				break;

			case Resizing_type::EXPAND_SNAPSHOT_BUF:

				gen_resizing_fs_tool_start_node(
					xml, _resizing_fs_tool, "ft",
					_extend_config->num_bytes / TRESOR_BLOCK_SIZE);

				break;

			default:

				class Unexpected_resizing_type { };
				throw Unexpected_resizing_type { };
				break;
			}
			break;

		case Resizing_state::IN_PROGRESS_AT_DEVICE:

			gen_resizing_fs_query_start_node(xml, _resizing_fs_query);
			break;

		case Resizing_state::DETERMINE_CLIENT_FS_SIZE:

			gen_client_fs_fs_query_start_node(xml, _client_fs_fs_query);
			break;

		case Resizing_state::RUN_RESIZE2FS:

			gen_resize2fs_start_node(xml, _resize2fs);
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

		if (_resizing_state == Resizing_state::INACTIVE ||
		    _resizing_type != Resizing_type::EXPAND_CLIENT_FS) {

			gen_policy_for_child_service(xml, "File_system", _rump_vfs);
			gen_rump_vfs_start_node(xml, _rump_vfs);
		}
		break;
	}
	case State::LOCK_ISSUE_DEINIT_REQUEST_AT_TRESOR:

		gen_parent_provides_and_report_nodes(xml);
		gen_policy_for_child_service(xml, "File_system", _rump_vfs);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_vfs_start_node(xml, _tresor_vfs, _tresor_image_file_name);
		gen_tresor_vfs_block_start_node(xml, _tresor_vfs_block);
		gen_snapshots_fs_query_start_node(xml, _snapshots_fs_query);
		gen_lock_fs_tool_start_node(xml, _lock_fs_tool);
		break;

	case State::LOCK_WAIT_TILL_DEINIT_REQUEST_IS_DONE:

		gen_parent_provides_and_report_nodes(xml);
		gen_policy_for_child_service(xml, "File_system", _rump_vfs);
		gen_tresor_trust_anchor_vfs_start_node(xml, _tresor_trust_anchor_vfs, _jent_avail);
		gen_tresor_vfs_start_node(xml, _tresor_vfs, _tresor_image_file_name);
		gen_tresor_vfs_block_start_node(xml, _tresor_vfs_block);
		gen_snapshots_fs_query_start_node(xml, _snapshots_fs_query);
		gen_lock_fs_query_start_node(xml, _lock_fs_query);
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


size_t Main::_tresor_size() const
{
	return
		_tresor_nr_of_blocks(
			TRESOR_NR_OF_SUPERBLOCKS,
			TRESOR_VBD_MAX_LVL + 1,
			TRESOR_VBD_DEGREE,
			_tresor_tree_num_leaves(_ui_config->client_fs_size),
			TRESOR_FREE_TREE_MAX_LVL + 1,
			TRESOR_FREE_TREE_DEGREE,
			_tresor_tree_num_leaves(_ui_config->journaling_buf_size))
		* TRESOR_BLOCK_SIZE;
}


size_t Main::_tresor_nr_of_blocks(size_t nr_of_superblocks,
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
	 * but the Tresor still uses an MT the same size as the FT for simplicity
	 * reasons. As soon as the Tresor does it right we should fix also this path.
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


void Component::construct(Genode::Env &env) { static File_vault::Main main { env }; }
