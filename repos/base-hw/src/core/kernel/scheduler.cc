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

#include <base/log.h>
#include <hw/assert.h>
#include <kernel/scheduler.h>

using namespace Kernel;


void Scheduler::_reset(Scheduling_context &context)
{
	context._claim = context._quota;
}


void Scheduler::print(Genode::Output &output) const
{
	Genode::print(output, "{\n"); 
	Genode::print(output, "   \"quota\": \"", _residual, "/", _quota, "\", ");
	Genode::print(output, "\"fill\": ", _fill, ", ");
	Genode::print(output, "\"need to schedule\": ", _need_to_schedule ? "true" : "false", ", ");
	Genode::print(output, "\"last_time\": ", _last_time, "");

	if (_head != nullptr) {

		Genode::print(output, ",\n   \"head\": { ");
		Genode::print(output, "\"id\": ", _head->_id, ", ");
		Genode::print(output, "\"quota\": ", _head_quota, ", ");
		Genode::print(output, "\"claims\": ", _head_claims ? "true" : "false", ", ");
		Genode::print(output, "\"yields\": ", _head_yields ? "true" : "false", " ");
		Genode::print(output, "}");
	}
	bool prios_empty { true };
	_for_each_prio([&] (Priority const prio, bool &) {
		if (prios_empty && (!_rcl[prio].empty() || !_ucl[prio].empty())) {
			prios_empty = false;
		}
	});
	if (!prios_empty) {

		Genode::print(output, ",\n   \"prios\": [");
		bool first_prio { true };
		_for_each_prio([&] (Priority const prio, bool &) {

			if (!_rcl[prio].empty() || !_ucl[prio].empty()) {

				if (first_prio) {
					first_prio = false;
				} else {
					Genode::print(output, ",");
				}
				Genode::print(output, "\n      { \"prio\": ", (unsigned)prio);
				if (!_rcl[prio].empty()) {

					Genode::print(output, ", \"ready\": [ ");
					bool first_ctx { true };
					_rcl[prio].for_each([&] (Scheduling_context const &context) {

						Genode::print(
							output, first_ctx ? "\"" : ", \"", context._id ,
							":", context._claim, "/", context._quota, "\"");

						first_ctx = false;
					});
					Genode::print(output, " ]");
				}
				if (!_ucl[prio].empty()) {

					Genode::print(output, ", \"unready\": [ ");
					bool first_ctx { true };
					_ucl[prio].for_each([&] (Scheduling_context const &context) {

						Genode::print(
							output, first_ctx ? "\"" : ", \"", context._id ,
							":", context._claim, "/", context._quota, "\"");

						first_ctx = false;
					});
					Genode::print(output, " ]");
				}
				Genode::print(output, " }");
			}
		});
		Genode::print(output, ",\n   ]");
	}
	if (!_fills.empty()) {

		Genode::print(output, ",\n   \"fills\": [ ");
		bool first_ctx { true };
		_fills.for_each([&] (Scheduling_context const &context) {

			if (first_ctx) {

				Genode::print(
					output, "\"", context._id, ":", context._fill, "\"");

				first_ctx = false;

			} else {

				Genode::print(output, ", \"", context._id, "\"");
			}
		});
		Genode::print(output, " ]\n");
	}
	Genode::print(output, "}");
}


void Scheduler::_reset_claims(unsigned const p)
{
	_rcl[p].for_each([&] (Scheduling_context &context) { _reset(context); });
	_ucl[p].for_each([&] (Scheduling_context &context) { _reset(context); });
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
	_fills.head_to_tail();
}


void Scheduler::_head_claimed(unsigned const r)
{
	if (!_head->_quota)
		return;

	_head->_claim = r > _head->_quota ? _head->_quota : r;

	if (_head->_claim || !_head->_ready)
		return;

	_rcl[_head->_prio].to_tail(&_head->_claim_item);
}


void Scheduler::_head_filled(unsigned const r)
{
	if (_fills.head() != &_head->_fill_item)
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
		Double_list_item<Scheduling_context> *const item { _rcl[p].head() };

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
	Double_list_item<Scheduling_context> *const item { _fills.head() };
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
		_rcl[context._prio].insert_tail(&context._claim_item);
	else
		_ucl[context._prio].insert_tail(&context._claim_item);
}


void Scheduler::_quota_revokation(Scheduling_context &context)
{
	if (context._ready)
		_rcl[context._prio].remove(&context._claim_item);
	else
		_ucl[context._prio].remove(&context._claim_item);
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


void Scheduler::update(time_t time)
{
	unsigned duration = (unsigned) (time - _last_time);
	_last_time        = time;
	_need_to_schedule = false;

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


void Scheduler::ready(Scheduling_context &context)
{
	assert(!context._ready && &context != &_idle);

	context._ready = 1;
	if (context._quota) {

		_ucl[context._prio].remove(&context._claim_item);
		if (context._claim) {

			_rcl[context._prio].insert_head(&context._claim_item);
			if (_head && _head_claims) {

				if (context._prio >= _head->_prio) {

					_need_to_schedule = true;
				}
			} else {

				_need_to_schedule = true;
			}
		} else {

			_rcl[context._prio].insert_tail(&context._claim_item);;
		}
	}

	context._fill = _fill;
	_fills.insert_tail(&context._fill_item);
	if (!_head || _head == &_idle) {

		_need_to_schedule = true;
	}
}


void Scheduler::unready(Scheduling_context &context)
{
	assert(context._ready && &context != &_idle);

	if (&context == _head)
		_need_to_schedule = true;

	context._ready = 0;
	_fills.remove(&context._fill_item);

	if (!context._quota)
		return;

	_rcl[context._prio].remove(&context._claim_item);
	_ucl[context._prio].insert_tail(&context._claim_item);
}


void Scheduler::yield()
{
	_head_yields = true;
	_need_to_schedule = true;
}


void Scheduler::remove(Scheduling_context &context)
{
	assert(&context != &_idle);

	if (context._ready) unready(context);

	if (&context == _head)
		_head = nullptr;

	if (!context._quota)
		return;

	_ucl[context._prio].remove(&context._claim_item);
}


void Scheduler::insert(Scheduling_context &context)
{
	assert(!context._ready);

	if (!context._quota)
		return;

	context._claim = context._quota;
	_ucl[context._prio].insert_head(&context._claim_item);
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
