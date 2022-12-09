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

#ifndef _CORE__KERNEL__SCHEDULER_H_
#define _CORE__KERNEL__SCHEDULER_H_

/* core includes */
#include <util.h>
#include <util/misc_math.h>
#include <kernel/configuration.h>
#include <kernel/double_list.h>

namespace Kernel {

	/**
	 * Priority of an unconsumed CPU claim versus other unconsumed CPU claims
	 */
	class Priority;

	/**
	 * Subject that participates in the scheduling for execution time
	 */
	class Scheduling_context;

	/**
	 * Schedules scheduling contexts for the execution time of a CPU
	 */
	class Scheduler;
}


class Kernel::Priority
{
	private:

		unsigned _value;

	public:

		static constexpr unsigned min() { return 0; }
		static constexpr unsigned max() { return cpu_priorities - 1; }

		/**
		 * Construct priority with value 'v'
		 */
		Priority(unsigned const v)
		:
			_value { Genode::min(v, max()) }
		{ }

		/*
		 * Standard operators
		 */

		Priority &operator =(unsigned const v)
		{
			_value = Genode::min(v, max());
			return *this;
		}

		operator unsigned() const { return _value; }
};


class Kernel::Scheduling_context
{
	friend class Scheduler;

	private:

		Double_list_item<Scheduling_context> _fill_item  { *this };
		Double_list_item<Scheduling_context> _claim_item { *this };
		Priority const                       _prio;
		unsigned                             _quota;
		unsigned                             _claim;
		unsigned                             _fill       { 0 };
		bool                                 _ready      { false };
		unsigned                             _id;

	public:

		/**
		 * Constructor
		 *
		 * \param p  claimed priority
		 * \param q  claimed quota
		 */
		Scheduling_context(Priority const p, unsigned const q, unsigned const id = 0)
		: _prio(p), _quota(q), _claim(q), _id(id) { }

		/*
		 * Accessors
		 */

		bool ready() const { return _ready; }
		void quota(unsigned const q) { _quota = q; }
};

class Kernel::Scheduler
{
	private:

		Double_list<Scheduling_context>  _rcl[Priority::max() + 1]; /* ready claims */
		Double_list<Scheduling_context>  _ucl[Priority::max() + 1]; /* unready claims */
		Double_list<Scheduling_context>  _fills { };          /* ready fills */
		Scheduling_context              &_idle;
		Scheduling_context              *_head = nullptr;
		unsigned                         _head_quota  = 0;
		bool                             _head_claims = false;
		bool                             _head_yields = false;
		unsigned const                   _quota;
		unsigned                         _residual;
		unsigned const                   _fill;
		bool                             _need_to_schedule { true };
		time_t                           _last_time { 0 };

		template <typename F> void _for_each_prio(F f) const
		{
			bool cancel_for_each_prio { false };
			for (unsigned p = Priority::max(); p != Priority::min() - 1; p--) {
				f(p, cancel_for_each_prio);
				if (cancel_for_each_prio)
					return;
			}
		}

		template <typename F> void _for_each_prio(F f)
		{
			bool cancel_for_each_prio { false };
			for (unsigned p = Priority::max(); p != Priority::min() - 1; p--) {
				f(p, cancel_for_each_prio);
				if (cancel_for_each_prio)
					return;
			}
		}

		static void _reset(Scheduling_context &context);

		void     _reset_claims(unsigned const p);
		void     _next_round();
		void     _consumed(unsigned const q);
		void     _set_head(Scheduling_context &context, unsigned const q, bool const c);
		void     _next_fill();
		void     _head_claimed(unsigned const r);
		void     _head_filled(unsigned const r);
		bool     _claim_for_head();
		bool     _fill_for_head();
		unsigned _trim_consumption(unsigned &q);

		void _quota_introduction(Scheduling_context &context);

		void _quota_revokation(Scheduling_context &context);

		void _quota_adaption(Scheduling_context &context, unsigned const q);

	public:

		/**
		 * Constructor
		 *
		 * \param i  Gets scheduled with static quota when no other scheduling
		 *           context can be scheduled. Unremovable. All values get
		 *           ignored.
		 * \param q  total amount of time quota that can be claimed by
		 *           scheduling contexts
		 * \param f  time-slice length of the fill round-robin
		 */
		Scheduler(Scheduling_context &i, unsigned const q, unsigned const f);

		bool need_to_schedule() { return _need_to_schedule; }
		void timeout()          { _need_to_schedule = true; }

		/**
		 * Update head according to the current (absolute) time
		 */
		void update(time_t time);

		/**
		 * Set scheduling context ready
		 */
		void ready(Scheduling_context &context);

		/**
		 * Set scheduling context unready
		 */
		void unready(Scheduling_context &context);

		/**
		 * Current head looses its current claim/fill for this round
		 */
		void yield();

		/**
		 * Remove scheduling context from scheduler
		 */
		void remove(Scheduling_context &context);

		/**
		 * Insert scheduling context into scheduler
		 */
		void insert(Scheduling_context &context);

		/**
		 * Set quota of scheduling context
		 */
		void quota(Scheduling_context &context, unsigned const q);

		/*
		 * Accessors
		 */

		Scheduling_context &head() const;
		unsigned head_quota() const {
			return Genode::min(_head_quota, _residual); }
		unsigned quota() const { return _quota; }
		unsigned residual() const { return _residual; }


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;
};

#endif /* _CORE__KERNEL__SCHEDULER_H_ */
