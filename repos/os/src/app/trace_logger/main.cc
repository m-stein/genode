/*
 * \brief  Log information about trace subjects
 * \author Martin Stein
 * \date   2018-01-15
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <trace_policy.h>
#include <monitor.h>

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/session_policy.h>
#include <timer_session/connection.h>

using namespace Genode;
using Thread_name = String<32>;


class Main
{
	private:

		enum { MAX_SUBJECTS      = 512 };
		enum { DEFAULT_PERIOD_MS = 5000 };

		struct No_matching_monitor : Exception { };

		Env                           &_env;
		Timer::Connection              _timer            { _env };
		Trace::Connection              _trace            { _env, 1024*1024*10, 64*1024, 0 };
		Attached_rom_dataspace         _config_rom       { _env, "config" };
		Xml_node                       _config           { _config_rom.xml() };
		bool const                     _affinity         { _config.attribute_value("affinity", false) };
		bool const                     _activity         { _config.attribute_value("activity", false) };
		bool                           _verbose          { _config.attribute_value("verbose",  false) };
		Microseconds                   _period_us        { _config.attribute_value("period_ms", (unsigned long)DEFAULT_PERIOD_MS) * 1000UL };
		Timer::Periodic_timeout<Main>  _period           { _timer, *this, &Main::_handle_period, _period_us };
		Heap                           _heap             { _env.ram(), _env.rm() };
		Monitor_tree                   _monitors         { };
		Trace_policy                   _policy           { _env, _trace,
		                                                   _config.attribute_value("trace_policy", Trace_policy::Name("null")) };
		unsigned long                  _report_id        { 0 };
		Trace::Subject_id              _subjects[MAX_SUBJECTS];
		Trace::Subject_id              _traced_subjects[MAX_SUBJECTS];
		unsigned long                  _num_subjects     { 0 };
		unsigned long                  _num_monitors     { 0 };

		void _handle_period(Duration)
		{
			/*
			 * Update monitors
			 *
			 * Which monitors are held and how they are configured depends on:
			 *
			 *   1) Which subjects are available at the Trace session,
			 *   2) which tracing state the subjects are currently in,
			 *   3) the configuration of this component about which subjects
			 *      to monitor and how
			 *
			 * All these might have changed since the last call of this method.
			 * So, adapt the monitors and the monitor tree accordingly.
			 */
			Monitor_tree new_monitors;
error("------------------------------------ ", &new_monitors, " - ", &_monitors);
			_num_subjects = _trace.subjects(_subjects, MAX_SUBJECTS);
			for (unsigned i = 0; i < _num_subjects; i++) {

				Trace::Subject_id const id = _subjects[i];
				if (_trace.subject_info(id).state() == Trace::Subject_info::DEAD)
{
error(id.id, " dead");
					continue;
}

				try {
					/* lookup monitor for subject ID */
					Monitor &monitor = _monitors.find_by_subject_id(id);

					/* if monitor is deprecated, leave it in the old tree */
					if (!_subject_matches_policy(id))
{
error(monitor.subject_id().id, " exists unwanted");
						continue;
}

error(monitor.subject_id().id, " exists wanted");
log(_monitors);
log("::: rm ", monitor.subject_id().id, " :::");
					/* move monitor from old to new tree */
					_monitors.remove(&monitor);
log(_monitors);
					new_monitors.insert(&monitor);
log("NEW");
log(new_monitors);

				} catch (Monitor_tree::No_match) {

					/* monitor only subjects the user is interested in */
					if (!_subject_matches_policy(id))
{
error(id.id, " new unwanted");
						continue;
}

error(id.id, " new wanted");
					/* create a monitor in the new tree for the subject */
					_new_monitor(new_monitors, id);
				}
			}
			/* all monitors in the old tree are deprecated, destroy them */
			while (Monitor *monitor = _monitors.first())
				_destroy_monitor(*monitor);

			/* override old tree with new tree */
			_monitors = new_monitors;
//error(__func__, __LINE__);

			/* dump information of each monitor left */
			log("");
			log("--- Report ", _report_id++, " (", _num_monitors, "/", _num_subjects, " subjects) ---");
			_monitors.for_each([&] (Monitor &monitor) {
				monitor.print(_activity, _affinity);
			});
//error(__func__, __LINE__);
		}

		void _destroy_monitor(Monitor &monitor)
		{
			if (_verbose)
				log("destroy monitor: subject ", monitor.subject_id().id);

			_trace.free(monitor.subject_id());
			_monitors.remove(&monitor);
			destroy(_heap, &monitor);
			_num_monitors--;
		}

		void _new_monitor(Monitor_tree &monitors, Trace::Subject_id id)
		{
			try { _trace.trace(id.id, _policy.id(), 16384U); }
			catch (Trace::Source_is_dead) {
				warning("Failed to enable tracing");
				return;
			}
			monitors.insert(new (_heap) Monitor(_trace, _env.rm(), id));
			_num_monitors++;
			if (_verbose)
				log("new monitor: subject ", id.id);
		}

		bool _subject_matches_policy(Trace::Subject_id id)
		{
			Trace::Subject_info info = _trace.subject_info(id);
			Session_label const label(info.session_label());
			try {
				Session_policy policy(label, _config);
				if (!policy.has_attribute("thread"))
					return true;

				return policy.attribute_value("thread", Thread_name()) ==
				       info.thread_name();
			}
			catch (Session_policy::No_policy_defined) { }
			return false;
		}

	public:

		Main(Env &env) : _env(env) { }
};


void Component::construct(Env &env) { static Main main(env); }








inline void My::Avl_node_base::_recompute_depth(Policy &policy)
{
	unsigned char old_depth = _depth;
	_depth = max(_child_depth(LEFT), _child_depth(RIGHT)) + 1;

	/* if our own value changes, update parent */
	if (_depth != old_depth && _parent)
		_parent->_recompute_depth(policy);

	/* call recompute hook only for valid tree nodes */
	if (_parent)
		policy.recompute(this);
}


void My::Avl_node_base::_adopt(My::Avl_node_base *node, Side i, Policy &policy)
{
	_child[i] = node;
	if (node)
		node->_parent = this;

	_recompute_depth(policy);
}


void My::Avl_node_base::_rotate_subtree(My::Avl_node_base *node, Side side, Policy &policy)
{
	int i = (node == _child[0]) ? LEFT : RIGHT;

	My::Avl_node_base *node_r   = node->_child[!side];
	My::Avl_node_base *node_r_l = node_r->_child[side];

	/* simple rotation */
	if (node_r->_bias() == !side) {

		node->_adopt(node_r_l, !side, policy);
		node_r->_adopt(node, side, policy);

		_adopt(node_r, i, policy);
	}

	/* double rotation */
	else if (node_r_l) {

		My::Avl_node_base *node_r_l_l = node_r_l->_child[side];
		My::Avl_node_base *node_r_l_r = node_r_l->_child[!side];

		node->_adopt(node_r_l_l, !side, policy);
		node_r->_adopt(node_r_l_r, side, policy);

		node_r_l->_adopt(node, side, policy);
		node_r_l->_adopt(node_r, !side, policy);

		_adopt(node_r_l, i, policy);
	}
}


void My::Avl_node_base::_rebalance_subtree(My::Avl_node_base *node, Policy &policy)
{
	int v = node->_child_depth(RIGHT) - node->_child_depth(LEFT);

	/* return if subtree is in balance */
	if (abs(v) < 2) return;

	_rotate_subtree(node, (v < 0), policy);
}


void My::Avl_node_base::insert(My::Avl_node_base *node, Policy &policy)
{
	if (node == this) {
		error("inserting element ", node, " twice into avl tree!");
		return;
	}

	Side i = LEFT;

	/* for non-root nodes, decide for a branch */
	if (_parent)
		i = policy.higher(this, node);

	if (_child[i])
		_child[i]->insert(node, policy);
	else
		_adopt(node, i, policy);

	/* the inserted node might have changed the depth of the subtree */
	_recompute_depth(policy);

	if (_parent)
		_parent->_rebalance_subtree(this, policy);
}


void My::Avl_node_base::remove(Policy &policy)
{
	My::Avl_node_base *lp = 0;
	My::Avl_node_base *l  = _child[0];

	if (!_parent)
		error("tried to remove AVL node that is not in an AVL tree");


	log(">>>A ", _id, " ", _parent->_id);
	if (_parent->_child[0])
		log(">>>A ", _id, " ", _parent->_id, "*", _parent, " L ", _parent->_child[0]->_id, "p", _parent->_child[0]->_parent->_id);
	if (_parent->_child[1])
		log(">>>A ", _id, " ", _parent->_id, "*", _parent, " R ", _parent->_child[1]->_id, "p", _parent->_child[1]->_parent->_id);

	if (l) {

		/* find right-most node in left sub tree (l) */
		while (l && l->_child[1])
			l = l->_child[1];

//log("rightmost ", l->_id);

		/* isolate right-most node in left sub tree */
		if (l == _child[0])
			_adopt(l->_child[0], LEFT, policy);
		else
			l->_parent->_adopt(l->_child[0], RIGHT, policy);

		/* consistent state */

		/* remember for rebalancing */
		if (l->_parent != this)
			lp = l->_parent;

		/* exchange this and l */
		for (int i = 0; i < 2; i++)
			if (_parent->_child[i] == this)
				_parent->_adopt(l, i, policy);

		l->_adopt(_child[0], LEFT, policy);
		l->_adopt(_child[1], RIGHT, policy);

	} else {

		/* no left sub tree, attach our right sub tree to our parent */
		for (int i = 0; i < 2; i++)
			if (_parent->_child[i] == this)
				_parent->_adopt(_child[1], i, policy);
	}



	/* walk the tree towards its root and rebalance sub trees */
	while (lp && lp->_parent) {
		My::Avl_node_base *lpp = lp->_parent;
		lpp->_rebalance_subtree(lp, policy);
		lp = lpp;
	}

	log(">>>B ", _id, " ", _parent->_id);
	if (_parent->_child[0])
		log(">>>B ", _id, " ", _parent->_id, "*", _parent, " L ", _parent->_child[0]->_id, "p", _parent->_child[0]->_parent->_id);
	if (_parent->_child[1])
		log(">>>B ", _id, " ", _parent->_id, "*", _parent, " R ", _parent->_child[1]->_id, "p", _parent->_child[1]->_parent->_id);

	/* reset node pointers */
	_child[LEFT] = _child[RIGHT] = 0;
	_parent = 0;
}


My::Avl_node_base::Avl_node_base(unsigned id) : _parent(0), _depth(1), _id(id) {
	_child[LEFT] = _child[RIGHT] = 0; }
