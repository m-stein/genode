/*
 * \brief   Schedules CPU shares for the execution time of a CPU
 * \author  Martin Stein
 * \date    2014-10-09
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/cpu_scheduler.h>
#include <assert.h>

using namespace Kernel;


void Cpu_scheduler::_reset(Claim * const c) {
	_share(c)->_claim = _share(c)->_quota; }


void Cpu_scheduler::_reset_claims(unsigned const p)
{
	_rcl[p].for_each([&] (Claim * const c) { _reset(c); });
	_ucl[p].for_each([&] (Claim * const c) { _reset(c); });
}


void Cpu_scheduler::_next_round()
{
	_residual = _quota;
	_for_each_prio([&] (unsigned const p) { _reset_claims(p); });
}


void Cpu_scheduler::_consumed(unsigned const q)
{
	if (_residual > q) { _residual -= q; }
	else { _next_round(); }
}


void Cpu_scheduler::_set_head(Share * const s, unsigned const q, bool const c)
{
	_head_quota = q;
	_head_claims = c;
	_head = s;
}


void Cpu_scheduler::_next_fill()
{
	_head->_fill = _fill;
	_fills.head_to_tail();
}


void Cpu_scheduler::_head_claimed(unsigned const r)
{
	if (!_head->_quota) { return; }
	_head->_claim = r > _head->_quota ? _head->_quota : r;
	if (_head->_claim || !_head->_ready) { return; }
	_rcl[_head->_prio].to_tail(_head);
}


void Cpu_scheduler::_head_filled(unsigned const r)
{
	if (_fills.head() != _head) { return; }
	if (r) { _head->_fill = r; }
	else { _next_fill(); }
}


bool Cpu_scheduler::_claim_for_head()
{
	for (signed p = Prio::MAX; p > Prio::MIN - 1; p--) {
		Share * const s = _share(_rcl[p].head());
		if (!s) { continue; }
		if (!s->_claim) { continue; }
		if (s != _head) { _turn_effect_share  (s->_claim, true, s); }
		else            { _turn_effect_timeout(s->_claim, true); }
		return 1;
	}
	return 0;
}


void Cpu_scheduler::_turn_effect_share(unsigned const q, bool const c, Share * const s)
{
	_head_quota  = q;
	_head_claims = c;
	_head        = s;
	_turn_effect = Turn_effect::SHARE;
}


void Cpu_scheduler::_turn_effect_timeout(unsigned const q, bool const c)
{
	_head_quota  = q;
	_head_claims = c;
	_turn_effect = Turn_effect::TIMEOUT;
}


bool Cpu_scheduler::_fill_for_head()
{
	Share * const s = _share(_fills.head());
	if (!s) { return false; }
	if (s != _head) { _turn_effect_share  (s->_fill, false, s); }
	else            { _turn_effect_timeout(s->_fill, false); }
	return true;
}


void Cpu_scheduler::_idle_for_head()
{
	if (_idle != _head) { _turn_effect_share  (_fill, false, _idle); }
	else                { _turn_effect_timeout(_fill, false); }
}


unsigned Cpu_scheduler::_trim_consumption(unsigned & q)
{
	q = Genode::min(Genode::min(q, _head_quota), _residual);
	return _head_quota - q;
}


void Cpu_scheduler::_quota_introduction(Share * const s)
{
	if (s->_ready) { _rcl[s->_prio].insert_tail(s); }
	else { _ucl[s->_prio].insert_tail(s); }
}


void Cpu_scheduler::_quota_revokation(Share * const s)
{
	if (s->_ready) { _rcl[s->_prio].remove(s); }
	else { _ucl[s->_prio].remove(s); }
}


void Cpu_scheduler::_quota_adaption(Share * const s, unsigned const q)
{
	if (q) { if (s->_claim > q) { s->_claim = q; } }
	else { _quota_revokation(s); }
}


void Cpu_scheduler::head_consumed(unsigned q)
{
	_head_consumed += q;
}


void Cpu_scheduler::_head_select()
{
	if (_claim_for_head()) { return; }
	if (_fill_for_head()) { return; }
	_idle_for_head();
}


void Cpu_scheduler::head_timeout()
{
	_head_flush_consumed();
	_head_select();
}


bool Cpu_scheduler::ready_remote(Share * const s1)
{
	_rrl.insert_tail(s1);

	/* share might be of any kind */
	Share * s2 = _head;
	if (!s1->_claim) { return s2 == _idle; }

	/* share has a claim */
	if (!_head_claims) { return true; }

	/* share and head have a different priority */
	if (s1->_prio < s2->_prio) { return false; }
	if (s1->_prio > s2->_prio) { return true; }

	/* share and head have the same priority */
	return false;
}


void Cpu_scheduler::_head_flush_consumed()
{
	unsigned q = _head_consumed;
	unsigned const r = _trim_consumption(q);
	if (_head_claims) { _head_claimed(r); }
	else              { _head_filled(r); }
	_consumed(q);
	_head_consumed = 0;
}


void Cpu_scheduler::ready(Share * const s1)
{
	assert(!s1->_ready && !s1->_ready_remote && s1 != _idle);
	s1->_ready = true;

	/* add fill to round robin */
	s1->_fill = _fill;
	_fills.insert_tail(s1);

	/* if available, move claim from unready to ready list */
	if (s1->_quota) {
		_ucl[s1->_prio].remove(s1);
		Claim_list * const rcl = &_rcl[s1->_prio];

		/*
		 * Insertions into the ready list follow these rules:
		 *
		 * 1) depleted claims gather at the tail to enable fast checking
		 *    whether there are schedulable claims for a certain priority
		 * 2) if the list head is not depleted, do not insert in front of it
		 *    because it might also be the current scheduling choice and would
		 *    then be interrupted unnecessarily
		 */
		if (s1->_claim) {
			Share * const s3 = _share(rcl->head());
			if (s3 && s3->_claim) { rcl->insert_behind_head(s1); }
			else                  { rcl->insert_head(s1); }
		} else                    { rcl->insert_tail(s1); }
	}

	/* share might be of any kind */
	Share * s2 = _head;
	if (!s1->_claim) {
		if (s2 != _idle) { return; }
		_head_flush_consumed();
		_turn_effect_share(s1->_fill, false, s1);
		return;
	}

	/* share has a claim */
	if (!_head_claims) {
		_head_flush_consumed();
		_turn_effect_share(s1->_claim, true, s1);
		return;
	}

	/* share and head have a different priority */
	if (s1->_prio < s2->_prio) { return; }
	if (s1->_prio > s2->_prio) {
		_head_flush_consumed();
		_turn_effect_share(s1->_claim, true, s1);
		return;
	}

	/* share and head have the same priority */
	for (; s2 && s2 != s1; s2 = _share(Claim_list::next(s2))) { }
	if (s2) { return; }
	_head_flush_consumed();
	_turn_effect_share(s1->_claim, true, s1);
}


void Cpu_scheduler::unready(Share * const s)
{
	assert(s->_ready && !s->_ready_remote && s != _idle);

	s->_ready = 0;

	/* remove fill */
	_fills.remove(s);

	/* if avaliable, move claim from ready to unready list */
	if (s->_quota) {
		_rcl[s->_prio].remove(s);
		_ucl[s->_prio].insert_tail(s);
	}

	if (s != _head) { return; }
	head_timeout();
}


void Cpu_scheduler::head_yields()
{
	unsigned q = _head_consumed;
	_trim_consumption(q);
	if (_head_claims) { _head_claimed(0); }
	else              { _head_filled(0); }
	_consumed(q);
	_head_consumed = 0;
	_head_select();
}


void Cpu_scheduler::dump()
{
	Genode::printf("  H time %u head ", _quota - _residual);
	_head->print_label();
	Genode::printf(" %s %u consumed %u effect %s\n",
		_head_claims ? "claims" : "fills",
		_head_quota, _head_consumed,
		_turn_effect == Turn_effect::NONE    ? "NONE"    :
		_turn_effect == Turn_effect::TIMEOUT ? "TIMEOUT" :
		_turn_effect == Turn_effect::SHARE   ? "SHARE"   : "?" );

	Genode::printf("  C ");
	_for_each_prio([&] (unsigned const p) {
		_rcl[p].for_each([&] (Claim * const c) {
			_share(c)->print_label();
			Genode::printf(":%u ", _share(c)->_claim);
		});
		_ucl[p].for_each([&] (Claim * const c) {
			_share(c)->print_label();
			Genode::printf(".%u ", _share(c)->_claim);
		});
		Genode::printf("- ");
	});
	Genode::printf("\n  F ");
	_fills.for_each([&] (Fill * const f) {
		_share(f)->print_label();
		if (f == _fills.head()) {
			Genode::printf("%s%u",
				_share(f)->_ready ? ":" : ".", _share(f)->_fill);
		}
		Genode::printf(" ");
	});
	Genode::printf("\n");
}


Cpu_scheduler::Turn_effect::Enum Cpu_scheduler::end_turn()
{
	/* flush ready remote list */
	_rrl.for_each([&] (Cpu_share_ready_remote * const r) {
		Share * const s = _share(r);
		ready(s);
		s->_ready_remote = false;
	});
	_rrl.remove_all();

	/* read and reset turn effect */
	Turn_effect::Enum e = _turn_effect;
	_turn_effect = Turn_effect::NONE;
	return e;
}


void Cpu_scheduler::remove(Share * const s)
{
	assert(s != _idle);

	/*
	 * FIXME
	 * Thanks to helping, this can happen and shall not be treated as bad
	 * behavior. But by now, we have no stable solution for it.
	 *
	 */
	if (s == _head) {
		PERR("Removing the head of the CPU scheduler isn't supported by now.");
		while (1) ;
	}
	if (s->_ready_remote) { _rrl.remove(s); }
	if (s->_ready) { _fills.remove(s); }
	if (!s->_quota) { return; }
	if (s->_ready) { _rcl[s->_prio].remove(s); }
	else { _ucl[s->_prio].remove(s); }
}


void Cpu_scheduler::insert(Share * const s)
{
	assert(!s->_ready);
	if (!s->_quota) { return; }
	s->_claim = s->_quota;
	_ucl[s->_prio].insert_head(s);
}


void Cpu_scheduler::quota(Share * const s, unsigned const q)
{
	assert(s != _idle);
	if (s->_quota) { _quota_adaption(s, q); }
	else if (q) { _quota_introduction(s); }
	s->_quota = q;
}


Cpu_scheduler::Cpu_scheduler(Share * const i, unsigned const q,
                             unsigned const f)
:
	_idle(i), _head_consumed(0), _quota(q), _residual(q),
	_fill(f), _turn_effect(Turn_effect::SHARE)
{
	_set_head(i, f, 0);
}
