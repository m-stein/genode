/*
 * \brief  Startup Wifi driver
 * \author Josef Soentgen
 * \date   2018-07-31
 *
 * This wifi driver front end uses the CTRL interface of the wpa_supplicant via
 * a Genode specific backend, which merely wraps a shared memory buffer, to
 * manage the supplicant.
 *
 * Depending on the 'wifi_config' ROM content it will instruct the supplicant
 * to enable, disable and connect to wireless networks. Commands and their
 * corresponding execute result are handled by the '_cmd_handler' dispatcher.
 * This handler drives the front end's state-machine. Any different type of
 * action can only be initiated from the 'IDLE' state. Unsolicited events, e.g.
 * a scan-results-available event, may influence the current state. Config
 * updates are deferred in case the current state is not 'IDLE'.
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _WIFI_FRONTEND_H_
#define _WIFI_FRONTEND_H_

/* Genode includes */
#include <libc/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <base/sleep.h>
#include <os/reporter.h>
#include <timer_session/connection.h>
#include <util/xml_node.h>

/* rep includes */
#include <wifi/ctrl.h>
#include <wifi/rfkill.h>

/* local includes */
#include <util.h>

/* declare manually as it is a internal hack^Winterface */
extern void wifi_kick_socketcall();


namespace Wifi {
	struct Frontend;
}


/* keep ordered! */
static struct Recv_msg_table {
	char const *string;
	size_t len;
} recv_table[] = {
	{ "OK",                           2 },
	{ "FAIL",                         4 },
	{ "CTRL-EVENT-SCAN-RESULTS",     23 },
	{ "CTRL-EVENT-CONNECTED",        20 },
	{ "CTRL-EVENT-DISCONNECTED",     23 },
	{ "SME: Trying to authenticate", 27 },
};

enum Rmi {
	OK = 0,
	FAIL,
	SCAN_RESULTS,
	CONNECTED,
	DISCONNECTED,
	SME_AUTH,
};


static inline bool check_recv_msg(char const *msg, Recv_msg_table const &entry)
{
	return Genode::strcmp(entry.string, msg, entry.len) == 0;
}


static bool cmd_successful(char const *msg)
{
	return check_recv_msg(msg, recv_table[OK]);
}


static bool cmd_fail(char const *msg)
{
	return check_recv_msg(msg, recv_table[FAIL]);
}


static bool results_available(char const *msg)
{
	return check_recv_msg(msg, recv_table[SCAN_RESULTS]);
}


static bool connecting_to_network(char const *msg)
{
	return check_recv_msg(msg, recv_table[SME_AUTH]);
}


static bool scan_results(char const *msg)
{
	return Genode::strcmp("bssid", msg, 5) == 0;
}


/*
 * Central network data structure
 */
struct Accesspoint
{
	using Bssid = Genode::String<17+1>;
	using Freq  = Genode::String<4+1>;
	using Prot  = Genode::String<7+1>;
	using Ssid  = Genode::String<32+1>;
	using Pass  = Genode::String<63+1>;

	/*
	 * Accesspoint information fields used by the front end
	 */
	Bssid    bssid  { };
	Freq     freq   { };
	Prot     prot   { };
	Ssid     ssid   { };
	Pass     pass   { };
	unsigned signal { 0 };

	/*
	 * CTRL interface fields
	 *
	 * The 'enabled' field is set to true if ENABLE_NETWORK
	 * was successfully executed. The network itself might
	 * get disabled by wpa_supplicant itself in case it cannot
	 * connect to the network, which will _not_ be reflected
	 * here.
	 */
	int  id      { -1 };
	bool enabled { false };

	/* 
	 * Internal configuration fields
	 */
	bool enable { false };
	bool update { false };
	bool stale  { false };

	/**
	 * Default constructor
	 */
	Accesspoint() { }

	/**
	 * Constructor that initializes information fields
	 */
	Accesspoint(char const *bssid, char const *freq,
	            char const *prot,  char const *ssid, unsigned signal)
	: bssid(bssid), freq(freq), prot(prot), ssid(ssid), signal(signal)
	{ }

	bool valid()  const { return ssid.length() > 1; }
	bool wpa()    const { return prot != "NONE"; }
	bool stored() const { return id != -1; }
};


template <typename FUNC>
static void for_each_result_line(char const *msg, FUNC const &func)
{
	char line_buffer[1024];
	size_t cur = 0;

	/* skip headline */
	size_t until = Util::next_char(msg, cur, '\n');
	cur += until + 1;

	while (msg[cur] != 0) {
		until = Util::next_char(msg, cur, '\n');
		Genode::memcpy(line_buffer, &msg[cur], until);
		line_buffer[until] = 0;
		cur += until + 1;

		char const *s[5] = { };

		for (size_t c = 0, i = 0; i < 5; i++) {
			size_t pos = Util::next_char(line_buffer, c, '\t');
			line_buffer[c+pos] = 0;
			s[i] = (char const*)&line_buffer[c];
			c += pos + 1;
		}

		bool const is_wpa1 = Util::string_contains((char const*)s[3], "WPA");
		bool const is_wpa2 = Util::string_contains((char const*)s[3], "WPA2");

		unsigned signal = Util::approximate_quality(s[2]);

		char const *prot = is_wpa1 ? "WPA" : "NONE";
		            prot = is_wpa2 ? "WPA2" : prot;

		Accesspoint ap(s[0], s[1], prot, s[4], signal);

		func(ap);
	}
}


/*
 * Wifi driver front end
 */
struct Wifi::Frontend
{
	/* accesspoint */

	enum { MAX_ACCESSPOINTS = 16, };
	Accesspoint _aps[MAX_ACCESSPOINTS];

	Accesspoint *_lookup_ap_by_ssid(Accesspoint::Ssid const &ssid)
	{
		for (Accesspoint &ap : _aps) {
			if (ap.valid() && ap.ssid == ssid) { return &ap; }
		}
		return nullptr;
	}

	Accesspoint *_lookup_ap_by_bssid(Accesspoint::Bssid const &bssid)
	{
		for (Accesspoint &ap : _aps) {
			if (ap.valid() && ap.bssid == bssid) { return &ap; }
		}
		return nullptr;
	}

	Accesspoint *_ap_slot()
	{
		for (Accesspoint &ap : _aps) {
			if (!ap.valid()) { return &ap; }
		}
		return nullptr;
	}

	void _free_ap_slot(Accesspoint &ap)
	{
		ap.ssid  = Accesspoint::Ssid("");
		ap.bssid = Accesspoint::Bssid("");
		ap.id    = -1;
	}

	template <typename FUNC>
	void _for_each_ap(FUNC const &func)
	{
		for (Accesspoint &ap : _aps) {
			if (!ap.valid()) { continue; }
			func(ap);
		}
	}

	unsigned _count_enabled()
	{
		unsigned count = 0;
		auto enabled = [&](Accesspoint const &ap) {
			count += ap.enabled;
		};
		_for_each_ap(enabled);
		return count;
	}

	unsigned _count_stored()
	{
		unsigned count = 0;
		auto enabled = [&](Accesspoint const &ap) {
			count += ap.stored();
		};
		_for_each_ap(enabled);
		return count;
	}

	/* remaining stuff */

	Msg_buffer _msg;

	Genode::Lock _notify_lock { Genode::Lock::UNLOCKED };

	void _notify_lock_lock()   { _notify_lock.lock(); }
	void _notify_lock_unlock() { _notify_lock.unlock(); }

	bool _rfkilled { false };

	Genode::Signal_handler<Wifi::Frontend> _rfkill_handler;

	void _handle_rfkill()
	{
		_rfkilled = wifi_get_rfkill();

		/* re-enable scan timer */
		if (!_rfkilled) {
			_scan_timer.sigh(_scan_timer_sigh);
			_arm_scan_timer(false);
		} else {
			_scan_timer.sigh(Genode::Signal_context_capability());
		}


		if (_rfkilled && _state != State::IDLE) {
			Genode::error("rfkilled with (", state_strings(_state), ")");
			_state_transition(_state, State::IDLE);
		}
	}

	/* config */

	Genode::Attached_rom_dataspace         _config_rom;
	Genode::Signal_handler<Wifi::Frontend> _config_sigh;

	bool _verbose       { false };
	bool _verbose_state { false };
	bool _use_11n       { true };

	bool _deferred_config_update { false };

	unsigned _connected_scan_interval { 15 };
	unsigned _scan_interval           {  5 };

	void _handle_config_update()
	{
		_config_rom.update();

		if (!_config_rom.valid()) { return; }

		Genode::Xml_node config = _config_rom.xml();

		_verbose       = config.attribute_value("verbose",       _verbose);
		_verbose_state = config.attribute_value("verbose_state", _verbose_state);

		/* only evaluated at start-up */
		_use_11n = config.attribute_value("use_11n", _use_11n);

		_connected_scan_interval =
			Util::check_time(config.attribute_value("connected_scan_interval",
			                                        _connected_scan_interval),
			                 15, 15*60);

		_scan_interval =
			Util::check_time(config.attribute_value("scan_interval",
			                                        _scan_interval),
			                 5, 15*60);

		/*
		 * Always handle rfkill, regardless in which state we are currently in.
		 * When we come back from rfkill, will most certainly will be IDLE anyway.
		 */
		if (config.has_attribute("rfkill")) {
			bool const blocked = config.attribute_value("rfkill", false);
			wifi_set_rfkill(blocked);

			/*
			 * In case we get blocked set rfkilled immediately to prevent
			 * any further scanning operation. The actual value will be set
			 * by the singal handler but is not expected to be any different
			 * as the rfkill call is not supposed to fail.
			 */
			if (blocked) { _rfkilled = true; }
		}

		/*
		 * Override any scanning activity, which might break horribly
		 * if we get the wrong response, e.g. scan results while we are
		 * waiting for ADD_NETWORK.
		 */
		if (_state != State::IDLE) {
			Genode::warning("deferring config update (", state_strings(_state), ")");
			_deferred_config_update = true;
			return;
		}

		/* update AP list */
		try {
			Genode::Xml_node aps = config.sub_node("accesspoints");

			aps.for_each_sub_node("accesspoint", [&] ( Genode::Xml_node node) {

				Accesspoint ap;
				ap.ssid  = node.attribute_value("ssid",  Accesspoint::Ssid(""));
				ap.bssid = node.attribute_value("bssid", Accesspoint::Bssid(""));

				size_t const ssid_len = ap.ssid.length() - 1;
				if (ssid_len == 0 || ssid_len > 32) {
					Genode::warning("ignoring accesspoint with invalid ssid");
					return;
				}

				Accesspoint *p = _lookup_ap_by_ssid(ap.ssid);
				if (p) {
					if (_verbose) { Genode::log("Update: '", p->ssid, "'"); }
				} else {
					p = _ap_slot();
					if (!p) {
						Genode::warning("could not add accesspoint, no slots left");
						return;
					}
				}

				ap.pass   = node.attribute_value("passphrase", Accesspoint::Pass(""));
				ap.prot   = node.attribute_value("protection", Accesspoint::Prot("NONE"));
				ap.enable = node.attribute_value("enabled",    false);

				size_t const psk_len = ap.pass.length() - 1;
				if (psk_len < 8 || psk_len > 63) {
					Genode::warning("ignoring accesspoint with invalid pass");
					return;
				}

				if (ap.pass != p->pass || ap.prot != p->prot) {
					p->update = true;
				}

				/* TODO add better way to check validity */
				if (ap.bssid.length() > 1) { p->bssid = ap.bssid; }

				p->ssid   = ap.ssid;
				p->prot   = ap.prot;
				p->pass   = ap.pass;
				p->enable = ap.enable;
			});
		} catch (...) { Genode::warning("accesspoint list empty"); }

		/*
		 * Marking removes stale APs first and triggers adding of
		 * new ones afterwards.
		 */
		_mark_stale_aps(config);
	}

	/* state */

	Accesspoint *_processed_ap { nullptr };
	Accesspoint *_connected_ap { nullptr };

	enum State {
		IDLE    = 0x00,
		SCAN    = 0x01,
		NETWORK = 0x02,
		CONNECT = 0x04,

		INITIATE_SCAN   = 0x00|SCAN,
		PENDING_RESULTS = 0x10|SCAN,

		ADD_NETWORK        = 0x00|NETWORK,
		FILL_NETWORK_SSID  = 0x10|NETWORK,
		FILL_NETWORK_PSK   = 0x20|NETWORK,
		REMOVE_NETWORK     = 0x30|NETWORK,
		ENABLE_NETWORK     = 0x40|NETWORK,
		DISABLE_NETWORK    = 0x50|NETWORK,
		DISCONNECT_NETWORK = 0x60|NETWORK,

		CONNECTING   = 0x00|CONNECT,
		CONNECTED    = 0x10|CONNECT,
		DISCONNECTED = 0x20|CONNECT,
	};

	State _state { State::IDLE };

	char const *state_strings(State state)
	{
		switch (state) {
		case IDLE:              return "idle";
		case INITIATE_SCAN:     return "initiate scan";
		case PENDING_RESULTS:   return "pending results";
		case ADD_NETWORK:       return "add network";
		case FILL_NETWORK_SSID: return "fill network ssid";
		case FILL_NETWORK_PSK:  return "fill network pass";
		case REMOVE_NETWORK:    return "remove network";
		case ENABLE_NETWORK:    return "enable network";
		case DISABLE_NETWORK:   return "disable network";
		case CONNECTING:        return "connecting";
		case CONNECTED:         return "connected";
		case DISCONNECTED:      return "disconnected";
		default:                return "unknown";
		};
	}

	void _state_transition(State &current, State next)
	{
		if (_verbose_state) {
			using namespace Genode;
			log("Transition: ", state_strings(current), " -> ",
			    state_strings(next));
		}

		current = next;
	}

	using Cmd_str = Genode::String<1024>;

	void _prepare_cmd(Cmd_str const &str)
	{
		Genode::memset(_msg.send, 0, sizeof(_msg.send));
		Genode::memcpy(_msg.send, str.string(), str.length());
		++_msg.send_id;

		wpa_ctrl_set_fd();
	}

	/* scan */

	Timer::Connection                      _scan_timer;
	Genode::Signal_handler<Wifi::Frontend> _scan_timer_sigh;

	void _handle_scan_timer()
	{
		/*
		 * If we are blocked or currently trying to join a network
		 * suspend scanning.
		 */
		if (_rfkilled || _connecting.length() > 1) { return; }

		_arm_scan_timer(!!_connected_ap);

		/* skip as we will be scheduled some time soon(tm) anyway */
		if (_state != State::IDLE) { return; }

		/* TODO scan request/pending results timeout */
		_state_transition(_state, State::INITIATE_SCAN);
		_prepare_cmd(Cmd_str("SCAN"));
	}

	void _arm_scan_timer(bool connected)
	{
		unsigned const sec = connected ? _connected_scan_interval : _scan_interval;
		_scan_timer.trigger_once(sec * (1000 * 1000));
	}

	Genode::Constructible<Genode::Reporter> _ap_reporter;

	void _report_scan_results(char const *msg)
	{
		for_each_result_line(msg, [&] (Accesspoint const &ap) {

			Accesspoint *p = _lookup_ap_by_ssid(ap.ssid);
			if (!p) { return; }

			/* TODO handle same ssid multiple bssid */
			if (p->bssid.length() < 17 && p->bssid != ap.bssid) {
				p->bssid = ap.bssid;
				p->freq  = ap.freq;
			}
		});

		try {
			Genode::Reporter::Xml_generator xml(*_ap_reporter, [&]() {

				for_each_result_line(msg, [&] (Accesspoint const &ap) {

					xml.node("accesspoint", [&]() {
						xml.attribute("ssid", ap.ssid);
						xml.attribute("bssid", ap.bssid);
						xml.attribute("freq", ap.freq);
						xml.attribute("quality", ap.signal);
						if (ap.wpa()) { xml.attribute("protection", ap.prot); }
					});
				});
			});
		} catch (...) { }

		_network_enable();
	}

	/* network commands */

	void _mark_stale_aps(Genode::Xml_node const &config)
	{
		try {
			Genode::Xml_node nodes = config.sub_node("accesspoints");

			auto mark_stale = [&] (Accesspoint &ap) {
				ap.stale = true;

				nodes.for_each_sub_node("accesspoint", [&] ( Genode::Xml_node node) {
					Accesspoint::Ssid ssid = node.attribute_value("ssid",  Accesspoint::Ssid(""));

					if (ap.ssid == ssid) { ap.stale = false; }
				});
			};

			_for_each_ap(mark_stale);
		} catch (Genode::Xml_node::Nonexistent_sub_node) {
			/* there are not accesspoints left, mark all */
			auto mark_all = [&] (Accesspoint &ap) {
				ap.stale = true;
			};
			_for_each_ap(mark_all);
		}

		_remove_stale_aps();
	}

	void _remove_stale_aps()
	{
		if (_state != State::IDLE) {
			Genode::warning("cannot remove stale APs in non-idle state "
			                "(", state_strings(_state), ")");
			return;
		}

		if (_processed_ap) { return; }

		for (Accesspoint &ap : _aps) {
			if (ap.valid() && ap.stale) {
				_processed_ap = &ap;
				break;
			}
		}

		if (!_processed_ap) {
			/* TODO move State transition somewhere more sane */
			_state_transition(_state, State::IDLE);
			_network_disable();
			return;
		}

		if (_verbose) {
			Genode::log("Remove network: '", _processed_ap->ssid, "'");
		}

		_state_transition(_state, State::REMOVE_NETWORK);
		_prepare_cmd(Cmd_str("REMOVE_NETWORK ", _processed_ap->id));
	}

	void _update_aps()
	{
		if (_state != State::IDLE) {
			Genode::warning("cannot enable network in non-idle state");
			return;
		}

		if (_processed_ap) { return; }

		for (Accesspoint &ap : _aps) {
			if (ap.valid() && ap.stored() && ap.update) {
				_processed_ap = &ap;
				break;
			}
		}

		if (!_processed_ap) { return; }

		if (_verbose) {
			Genode::log("Update network: '", _processed_ap->ssid, "'");
		}

		_processed_ap->update = false;

		/* re-use state to change PSK */
		_state_transition(_state, State::FILL_NETWORK_PSK);
		_network_set_psk();
	}


	void _add_new_aps()
	{
		if (_state != State::IDLE) {
			Genode::warning("cannot enable network in non-idle state");
			return;
		}

		if (_processed_ap) { return; }

		for (Accesspoint &ap : _aps) {
			if (ap.valid() && !ap.stored()) {
				_processed_ap = &ap;
				break;
			}
		}

		if (!_processed_ap) {
			/* XXX move State transition somewhere more sane */
			_state_transition(_state, State::IDLE);
			_update_aps();
			return;
		}

		if (_verbose) {
			Genode::log("Add network: '", _processed_ap->ssid, "'");
		}

		_state_transition(_state, State::ADD_NETWORK);
		_prepare_cmd(Cmd_str("ADD_NETWORK"));
	}

	void _network_enable()
	{
		if (_state != State::IDLE) {
			Genode::warning("cannot enable network in non-idle state");
			return;
		}

		if (_processed_ap) { return; }

		for (Accesspoint &ap : _aps) {
			if (    ap.valid()
			    && (ap.bssid.length() > 1)
			    && !ap.enabled && ap.enable) {
				_processed_ap = &ap;
				break;
			}
		}

		if (!_processed_ap) { return; }

		if (_verbose) {
			Genode::log("Enable network: '", _processed_ap->ssid, "'");
		}

		_state_transition(_state, State::ENABLE_NETWORK);
		_prepare_cmd(Cmd_str("ENABLE_NETWORK ", _processed_ap->id));
	}

	void _network_disable()
	{
		if (_state != State::IDLE) {
			Genode::warning("cannot enable network in non-idle state");
			return;
		}

		if (_processed_ap) { return; }

		for (Accesspoint &ap : _aps) {
			if (ap.valid() && ap.enabled && !ap.enable) {
				_processed_ap = &ap;
				break;
			}
		}

		if (!_processed_ap) {
			/* XXX move State transition somewhere more sane */
			_state_transition(_state, State::IDLE);
			_add_new_aps();
			return;
		}

		if (_verbose) {
			Genode::log("Disable network: '", _processed_ap->ssid, "'");
		}

		_state_transition(_state, State::DISABLE_NETWORK);
		_prepare_cmd(Cmd_str("DISABLE_NETWORK ", _processed_ap->id));
	}

	void _network_disconnect()
	{
		_state_transition(_state, State::DISCONNECT_NETWORK);
		_prepare_cmd(Cmd_str("DISCONNECT"));
	}

	void _network_set_ssid(char const *msg)
	{
		long id = -1;
		Genode::ascii_to(msg, id);

		_processed_ap->id = id;
		_prepare_cmd(Cmd_str("SET_NETWORK ", _processed_ap->id,
		                     " ssid \"", _processed_ap->ssid, "\""));
	}

	void _network_set_psk()
	{
		if (_processed_ap->wpa()) {
			_prepare_cmd(Cmd_str("SET_NETWORK ", _processed_ap->id,
			                     " psk \"", _processed_ap->pass, "\""));
		} else {
			_prepare_cmd(Cmd_str("SET_NETWORK ", _processed_ap->id,
			                     " key_mgmt NONE"));
		}
	}

	/* result handling */

	void _handle_scan_results(State state, char const *msg)
	{
		switch (state) {
		case State::INITIATE_SCAN:
			if (!cmd_successful(msg)) {
				Genode::warning("could not initiate scan: ", msg);
			}
			_state_transition(_state, State::IDLE);
			break;
		case State::PENDING_RESULTS:
			if (scan_results(msg)) {
				_state_transition(_state, State::IDLE);
				_report_scan_results(msg);
			}
			break;
		default:
			Genode::warning("unknown SCAN state: ", msg);
			break;
		}
	}

	void _handle_network_results(State state, char const *msg)
	{
		switch (state) {
		case State::ADD_NETWORK:
			if (cmd_fail(msg)) {
				Genode::error("could not add network: ", msg);
				_state_transition(_state, State::IDLE);
			} else {
				_state_transition(_state, State::FILL_NETWORK_SSID);
				_network_set_ssid(msg);
			}
			break;
		case State::REMOVE_NETWORK:
			_state_transition(_state, State::IDLE);

			if (cmd_fail(msg)) {
				Genode::error("could not remove network: ", msg);
			} else {
				_free_ap_slot(*_processed_ap);

				/* trigger the next round */
				_processed_ap = nullptr;
				_remove_stale_aps();
			}
			break;
		case State::FILL_NETWORK_SSID:
			_state_transition(_state, State::IDLE);

			if (!cmd_successful(msg)) {
				Genode::error("could not set ssid for network: ", msg);
				_state_transition(_state, State::IDLE);
			} else {
				/* trigger the next round */
				_processed_ap = nullptr;
				_add_new_aps();
			}
			break;
		case State::FILL_NETWORK_PSK:
			_state_transition(_state, State::IDLE);

			if (!cmd_successful(msg)) {
				Genode::error("could not set passphrase for network: ", msg);
			} else {
				/* trigger the next round */
				_processed_ap = nullptr;
				_add_new_aps();
			}
			break;
		case State::ENABLE_NETWORK:
			_state_transition(_state, State::IDLE);

			if (!cmd_successful(msg)) {
				Genode::error("could not enable network: ", msg);
			} else {
				_processed_ap->enabled = true;

				/* trigger the next round */
				_processed_ap = nullptr;
				_network_enable();
			}
			break;
		case State::DISABLE_NETWORK:
			_state_transition(_state, State::IDLE);

			if (!cmd_successful(msg)) {
				Genode::error("could not disable network: ", msg);
			} else {
				_processed_ap->enabled = false;

				/* XXX mostly false as _connected_ap is probably
				 *     reset at this point
				 */
				bool const joined = _connected_ap == _processed_ap;

				/* trigger the next round */
				_processed_ap = nullptr;
				if (joined) { _network_disconnect(); }
				else        { _network_disable(); }
			}
			break;
		case State::DISCONNECT_NETWORK:
			_state_transition(_state, State::IDLE);

			if (!cmd_successful(msg)) {
				Genode::error("could not disconnect from network: ", msg);
			} else {
				_network_disable();
			}
			break;
		default:
			Genode::warning("unknown network state: ", msg);
			break;
		}
	}

	/* connection state */

	Genode::Constructible<Genode::Reporter> _state_reporter { };

	Accesspoint::Bssid _connecting { };

	Accesspoint::Bssid const _extract_bssid(char const *msg, State state)
	{
		char bssid[32] = { };
		/* by the power of wc -c, I have the start pos... */
		enum { BSSID_CONNECT = 37, BSSID_DISCONNECT = 30, BSSID_CONNECTING = 33, };

		bool const connected  = state == State::CONNECTED;
		bool const connecting = state == State::CONNECTING;

		size_t const len   = 17;
		size_t const start = connected ? BSSID_CONNECT
		                               : connecting ? BSSID_CONNECTING
		                                            : BSSID_DISCONNECT;
		Genode::memcpy(bssid, msg + start, len);
		return Accesspoint::Bssid((char const*)bssid);
	}

	bool _auth_failure(char const *msg)
	{
		enum { REASON_OFFSET = 55, };
		unsigned reason = 0;
		Genode::ascii_to((msg + REASON_OFFSET), reason);
		switch (reason) {
		case  2: /* prev auth no longer valid */
		case 15: /* 4-way handshake timeout/failed */
			return true;
		default:
			return false;
		}
	}

	void _generate_state_report(char const *msg, State state)
	{
		bool const connected    = state == State::CONNECTED;
		bool const connecting   = state == State::CONNECTING;
		bool const disconnected = !connected && !connecting;

		Accesspoint::Bssid const &bssid = _extract_bssid(msg, state);
		Accesspoint *ap = _lookup_ap_by_bssid(bssid);
		if (!ap) {
			Genode::warning("could not look up accesspoint: ", bssid, " '", msg, "'");
			return;
		}

		_connecting   = connecting ? bssid : Accesspoint::Bssid();
		_connected_ap = connected  ? ap    : nullptr;

		bool const fail = disconnected && _auth_failure(msg);

		if (_verbose) {
			Genode::log(connected ? "Connected to:"
			                      : connecting ? "Connecting to:"
			                                   : "Disconnected from:",
			            " '", ap->ssid, "'", " ", ap->bssid, " (", ap->freq, ")",
			            fail ? " (auth failed)" : "");
		}

		try {
			Genode::Reporter::Xml_generator xml(*_state_reporter, [&] () {
				xml.node("accesspoint", [&] () {
					xml.attribute("ssid",  ap->ssid);
					xml.attribute("bssid", ap->bssid);
					xml.attribute("freq",  ap->freq);
					xml.attribute("state", connected ? "connected"
					                                 : connecting ? "connecting"
					                                              : "disconnected");
					xml.attribute("rfkilled", _rfkilled);
					xml.attribute("auth_failure", fail);
				});
			});
		} catch (...) { }

		/* kick scan */
		if (disconnected) { _handle_scan_timer(); }
	}

	/* events */

	bool _handle_connection_events(char const *msg)
	{
		bool const connected    = check_recv_msg(msg, recv_table[Rmi::CONNECTED]);
		bool const disconnected = check_recv_msg(msg, recv_table[Rmi::DISCONNECTED]);

		if      (connected)    { _generate_state_report(msg, State::CONNECTED); }
		else if (disconnected) { _generate_state_report(msg, State::DISCONNECTED); }

		return connected || disconnected;
	}

	Genode::Signal_handler<Wifi::Frontend> _events_handler;

	unsigned _last_event_id { 0 };

	void _handle_events()
	{
		char const *msg = reinterpret_cast<char const*>(_msg.event);
		unsigned const event_id = _msg.event_id;

		/* return early */
		if (_last_event_id == event_id) {
			_notify_lock_unlock();
			return;
		}

		if (results_available(msg)) {

			/*
			 * We might have to pull the socketcall task out of poll_all()
			 * because otherwise we might be late and wpa_supplicant has
			 * already removed all scan results due to BSS age settings.
			 */
			wifi_kick_socketcall();

			if (_state == State::IDLE) {
				_state_transition(_state, State::PENDING_RESULTS);
				_prepare_cmd(Cmd_str("SCAN_RESULTS"));
			}
		} else

		if (connecting_to_network(msg)) {
			_generate_state_report(msg, State::CONNECTING);
		} else

		{
			_handle_connection_events(msg);
		}

		_notify_lock_unlock();
	}

	Genode::Signal_handler<Wifi::Frontend> _cmd_handler;

	unsigned _last_recv_id { 0 };

	void _handle_cmds()
	{
		char const *msg = reinterpret_cast<char const*>(_msg.recv);
		unsigned const recv_id = _msg.recv_id;

		/* return early */
		if (_last_recv_id == recv_id) {
			_notify_lock_unlock();
			return;
		}

		_last_recv_id = recv_id;

		switch (_state & 0xf) {
		case State::SCAN:
			_handle_scan_results(_state, msg);
			break;
		case State::NETWORK:
			_handle_network_results(_state, msg);
			break;
		default:
		case State::IDLE:
			break;
		}
		_notify_lock_unlock();

		if (_verbose_state) {
			Genode::log("State:",
			            " connected: ", !!_connected_ap,
			            " connecting: ", _connecting.length() > 1,
			            " enabled: ",    _count_enabled(),
			            " stored: ",     _count_stored(),
			"");
		}

		if (_state == State::IDLE && _deferred_config_update) {
			_deferred_config_update = false;
			_handle_config_update();
		}
	}

	/**
	 * Constructor
	 */
	Frontend(Genode::Env &env)
	:
		_events_handler(env.ep(), *this, &Wifi::Frontend::_handle_events),
		_cmd_handler(env.ep(),    *this, &Wifi::Frontend::_handle_cmds),
		_rfkill_handler(env.ep(), *this, &Wifi::Frontend::_handle_rfkill),
		_config_rom(env, "wifi_config"),
		_config_sigh(env.ep(), *this, &Wifi::Frontend::_handle_config_update),
		_scan_timer(env),
		_scan_timer_sigh(env.ep(), *this, &Wifi::Frontend::_handle_scan_timer)
	{
		_config_rom.sigh(_config_sigh);
		_scan_timer.sigh(_scan_timer_sigh);

		try {
			_ap_reporter.construct(env, "accesspoints");
			_ap_reporter->enabled(true);
		} catch (...) {
			Genode::warning("no Report session available, scan results will "
			                "not be reported");
		}

		try {
			_state_reporter.construct(env, "state");
			_state_reporter->enabled(true);
		} catch (...) {
			Genode::warning("no Report session available, connectivity will "
			                "not be reported");
		}

		/* read in list of APs */
		_handle_config_update();

		/* kick-off initial scanning */
		_handle_scan_timer();
	}

	/**
	 *  Return if 11n operation is enabled
	 */
	bool use_11n() const { return _use_11n; }

	/**
	 * Get RFKILL signal capability
	 *
	 * Used by the wifi_drv to notify front end.
	 */
	Genode::Signal_context_capability rfkill_sigh()
	{
		return _rfkill_handler;
	}

	/**
	 * Get result signal capability
	 *
	 * Used by the wpa_supplicant to notify front end after processing
	 * a command.
	 */
	Genode::Signal_context_capability result_sigh()
	{
		return _cmd_handler;
	}

	/**
	 * Get event signal capability
	 *
	 * Used by the wpa_supplicant to notify front whenever a event
	 * was triggered.
	 */
	Genode::Signal_context_capability event_sigh()
	{
		return _events_handler;
	}

	/**
	 * Block until events were handled by the front end
	 *
	 * Used by the wpa_supplicant to wait for the front end.
	 */
	void block_for_processing() { _notify_lock_lock(); }

	/**
	 * Return shared memory message buffer
	 *
	 * Used for communication between front end and wpa_supplicant.
	 */
	Msg_buffer &msg_buffer() { return _msg; }
};

#endif /* _WIFI_FRONTEND_H_ */
