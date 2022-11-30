/*
 * \brief   Schedules scheduling contexts for the execution time of a CPU
 * \author  Martin Stein
 * \date    2014-10-09
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base/include */
#include <base/log.h>

/* base-hw/src/include */
#include <hw/assert.h>

/* base-hw/src/core */
#include <kernel/cpu_scheduler.h>

using namespace Kernel;


void Scheduler::_reset(Scheduling_context &context)
{
	context._claim = context._quota;
}


void Scheduler::_reset_claims(unsigned const p)
{
	_ready_claims[p].for_each([&] (Scheduling_context &context) { _reset(context); });
	_unready_claims[p].for_each([&] (Scheduling_context &context) { _reset(context); });
}


void Scheduler::_next_round()
{
	_residual = _quota;
	_for_each_prio([&] (Priority const p, bool &) { _reset_claims(p); });
}


void Scheduler::_consumed(unsigned const q)
{
	if (_residual > q)
		_residual -= q;
	else
		_next_round();
}


void Scheduler::_set_head(Scheduling_context &context, unsigned const q, bool const c)
{
	_head_quota = q;
	_head_claims = c;
	_head = &context;
}


void Scheduler::_next_fill()
{
	_head->_fill = _fill;
	_ready_fills.head_to_tail();
}


void Scheduler::_head_claimed(unsigned const r)
{
	if (!_head->_quota)
		return;

	_head->_claim = r > _head->_quota ? _head->_quota : r;

	if (_head->_claim || !_head->_ready)
		return;

	_ready_claims[_head->_prio].to_tail(&_head->_claim_item);
}


void Scheduler::_head_filled(unsigned const r)
{
	if (_ready_fills.head() != &_head->_fill_item)
		return;

	if (r)
		_head->_fill = r;
	else
		_next_fill();
}


bool Scheduler::_claim_for_head()
{
	bool result { false };
	_for_each_prio([&] (Priority const p, bool &cancel_for_each_prio) {
		Double_list_item<Scheduling_context> *const item { _ready_claims[p].head() };

		if (!item)
			return;

		Scheduling_context &context { item->payload() };

		if (!context._claim)
			return;

		_set_head(context, context._claim, 1);
		result = true;
		cancel_for_each_prio = true;
	});
	return result;
}


bool Scheduler::_fill_for_head()
{
	Double_list_item<Scheduling_context> *const item { _ready_fills.head() };
	if (!item)
		return 0;

	Scheduling_context &context = item->payload();
	_set_head(context, context._fill, 0);
	return 1;
}


unsigned Scheduler::_trim_consumption(unsigned &q)
{
	q = Genode::min(Genode::min(q, _head_quota), _residual);
	if (!_head_yields)
		return _head_quota - q;

	_head_yields = false;
	return 0;
}


void Scheduler::_quota_introduction(Scheduling_context &context)
{
	if (context._ready)
		_ready_claims[context._prio].insert_tail(&context._claim_item);
	else
		_unready_claims[context._prio].insert_tail(&context._claim_item);
}


void Scheduler::_quota_revokation(Scheduling_context &context)
{
	if (context._ready)
		_ready_claims[context._prio].remove(&context._claim_item);
	else
		_unready_claims[context._prio].remove(&context._claim_item);
}


void Scheduler::_quota_adaption(Scheduling_context &context, unsigned const q)
{
	if (q) {
		if (context._claim > q)
			context._claim = q;
	} else {
		_quota_revokation(context);
	}
}


void Scheduler::update_head(time_t time)
{
	unsigned duration = (unsigned) (time - _time_at_last_update);
	_time_at_last_update = time;
	_head_outdated = false;

	/* do not detract the quota if the head context was removed even now */
	if (_head) {
		unsigned const r = _trim_consumption(duration);

		if (_head_claims)
			_head_claimed(r);
		else
			_head_filled(r);

		_consumed(duration);
	}

	if (_claim_for_head())
		return;

	if (_fill_for_head())
		return;

	_set_head(_idle, _fill, 0);
}


void Scheduler::ready_check(Scheduling_context &s1)
{
	assert(_head);

	ready(s1);

	if (_head_outdated)
		return;

	Scheduling_context * s2 = _head;
	if (!s1._claim) {
		_head_outdated = s2 == &_idle;
	} else if (!_head_claims) {
		_head_outdated = true;
	} else if (s1._prio != s2->_prio) {
		_head_outdated = s1._prio > s2->_prio;
	} else {
		for (
			; s2 && s2 != &s1;
			s2 =
				Double_list<Scheduling_context>::next(&s2->_claim_item) != nullptr ?
					&Double_list<Scheduling_context>::next(&s2->_claim_item)->payload() :
					nullptr) ;

		_head_outdated = !s2;
	}
}


void Scheduler::ready(Scheduling_context &context)
{
	assert(!context._ready && &context != &_idle);

	_head_outdated = true;

	context._ready = 1;
	context._fill = _fill;
	_ready_fills.insert_tail(&context._fill_item);

	if (!context._quota)
		return;

	_unready_claims[context._prio].remove(&context._claim_item);

	if (context._claim)
		_ready_claims[context._prio].insert_head(&context._claim_item);
	else
		_ready_claims[context._prio].insert_tail(&context._claim_item);
}


void Scheduler::unready(Scheduling_context &context)
{
	assert(context._ready && &context != &_idle);

	_head_outdated = true;

	context._ready = 0;
	_ready_fills.remove(&context._fill_item);

	if (!context._quota)
		return;

	_ready_claims[context._prio].remove(&context._claim_item);
	_unready_claims[context._prio].insert_tail(&context._claim_item);
}


void Scheduler::yield()
{
	_head_yields = true;
	_head_outdated = true;
}


void Scheduler::remove(Scheduling_context &context)
{
	assert(&context != &_idle);

	_head_outdated = true;

	if (&context == _head)
		_head = nullptr;

	if (context._ready)
		_ready_fills.remove(&context._fill_item);

	if (!context._quota)
		return;

	if (context._ready)
		_ready_claims[context._prio].remove(&context._claim_item);
	else
		_unready_claims[context._prio].remove(&context._claim_item);
}


void Scheduler::insert(Scheduling_context &context)
{
	assert(!context._ready);

	_head_outdated = true;

	if (!context._quota)
		return;

	context._claim = context._quota;
	_unready_claims[context._prio].insert_head(&context._claim_item);
}


void Scheduler::quota(Scheduling_context &context, unsigned const q)
{
	assert(&context != &_idle);

	if (context._quota)
		_quota_adaption(context, q);
	else if (q)
		_quota_introduction(context);

	context._quota = q;
}


Scheduling_context &Scheduler::head() const
{
	assert(_head);
	return *_head;
}


Scheduler::Scheduler(Scheduling_context &i, unsigned const q, unsigned const f)
:
	_idle(i), _quota(q), _residual(q), _fill(f)
{
	_set_head(i, f, 0);
}
