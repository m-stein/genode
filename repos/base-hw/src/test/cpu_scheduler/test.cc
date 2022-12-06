/*
 * \brief  Test CPU-scheduler implementation of the kernel
 * \author Martin Stein
 * \date   2014-09-30
 */

/*
 * Copyright (C) 2014-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/construct_at.h>
#include <base/component.h>

/* core includes */
#include <kernel/cpu_scheduler.h>

using namespace Genode;
using namespace Kernel;

namespace Cpu_scheduler_test {

	class Main;
}


class Cpu_scheduler_test::Main
{
	private:

		enum { MAX_NR_OF_SHARES = 10 };
		enum { IDLE_SHARE_ID = 0 };
		enum { INVALID_SHARE_ID = MAX_NR_OF_SHARES };

		Env                      &_env;
		Constructible<Cpu_share>  _shares[MAX_NR_OF_SHARES] { };
		Cpu_scheduler             _scheduler;
		time_t                    _current_time { 0 };

		Cpu_share & _idle_share()
		{
			if (!_shares[IDLE_SHARE_ID].constructed()) {

				_shares[IDLE_SHARE_ID].construct(0, 0);
			}
			return *_shares[IDLE_SHARE_ID];
		}

		unsigned _id_of_share(Cpu_share const &share)
		{
			unsigned result { INVALID_SHARE_ID };
			for (unsigned id { 0 }; id < sizeof(_shares)/sizeof(_shares[0]);
			     id++) {

				if (_shares[id].constructed() &&
				    (&*_shares[id] == &share)) {

					result = id;
				}
			}
			if (result == INVALID_SHARE_ID) {

				error("failed to determine ID of share");
				_env.parent().exit(-1);
			}
			return 0;
		}

		Cpu_share & _share(unsigned const id)
		{
			if (id >= sizeof(_shares)/sizeof(_shares[0])) {

				error("failed to determine share by ID");
				_env.parent().exit(-1);
			}
			return *_shares[id];
		}

		void _construct_share(unsigned const id,
		                      unsigned const line_nr)
		{
			if (id == IDLE_SHARE_ID ||
			    id >= sizeof(_shares)/sizeof(_shares[0])) {

				error("failed to construct share in line ", line_nr);
				_env.parent().exit(-1);
			}
			if (_shares[id].constructed()) {

				error("failed to construct share in line ", line_nr);
				_env.parent().exit(-1);
			}
			unsigned prio  { 0 };
			unsigned quota { 0 };
			switch (id) {
				case 1: prio = 2; quota = 230; break;
				case 2: prio = 0; quota = 170; break;
				case 3: prio = 3; quota = 110; break;
				case 4: prio = 1; quota =  90; break;
				case 5: prio = 3; quota = 120; break;
				case 6: prio = 3; quota =   0; break;
				case 7: prio = 2; quota = 180; break;
				case 8: prio = 2; quota = 100; break;
				case 9: prio = 2; quota =   0; break;
				default:

					error("failed to construct share in line ", line_nr);
					_env.parent().exit(-1);
					break;
			}
			_shares[id].construct(prio, quota);
			_scheduler.insert(*_shares[id]);
		}

		void _destroy_share(unsigned const id)
		{
			if (id == IDLE_SHARE_ID ||
			    id >= sizeof(_shares)/sizeof(_shares[0])) {

				error("failed to destroy share");
				_env.parent().exit(-1);
			}
			_scheduler.remove(_share(id));
			_shares[id].destruct();
		}

		void _update_head_and_check(unsigned const consumed_quota,
		                            unsigned const expected_round_time,
		                            unsigned const expected_head_id,
		                            unsigned const expected_head_quota,
		                            unsigned const line_nr)
		{
			_current_time += consumed_quota;
			_scheduler.update(_current_time);
			unsigned const round_time {
				_scheduler.quota() - _scheduler.residual() };

			if (round_time != expected_round_time) {

				error("wrong time ", round_time, " in line ", line_nr);
				_env.parent().exit(-1);
			}
			Cpu_share &head { _scheduler.head() };
			unsigned const head_quota { _scheduler.head_quota() };
			if (&head != &_share(expected_head_id)) {

				error("wrong share ", _id_of_share(head), " in line ",
				      line_nr);

				_env.parent().exit(-1);
			}
			if (head_quota != expected_head_quota) {

				error("wrong quota ", head_quota, " in line ", line_nr);
				_env.parent().exit(-1);
			}
		}

		void _set_share_ready_and_check(unsigned const line_nr,
		                                unsigned const share_id,
		                                bool     const expect_head_outdated)
		{
			_scheduler.ready(_share(share_id));
			if (_scheduler.need_to_schedule() != expect_head_outdated) {

				error("wrong check result ",
				      _scheduler.need_to_schedule(), " in line ", line_nr);

				_env.parent().exit(-1);
			}
		}

	public:

		Main(Env &env);
};



/*
 * Shortcuts for all basic operations that the test consists of
 */

#define C(s)       _construct_share(s);
#define D(s)       _destroy_share(s);
#define A(s)       _scheduler.ready(_share(s));
#define I(s)       _scheduler.unready(_share(s));
#define Y          _scheduler.yield();
#define Q(s, q)    _scheduler.quota(_share(s), q);
#define U(c, t, s, q) _update_head_and_check(__LINE__, c, t, s, q);
#define O(s)       _set_share_ready_and_check(__LINE__, s, true);
#define N(s)       _set_share_ready_and_check(__LINE__, s, false);



Cpu_scheduler_test::Main::Main(Env &env)
:
	_env      { env },
	_scheduler { _idle_share(), 1000, 100 }
{
	/*
	 * Step-by-step testing
	 *
	 * Every line in this test is structured according to the scheme
	 * '<ops> U(t,c,s,q) <doc>' where the symbols are defined as follows:
	 *
	 * ops  Operations that affect the schedule but not the head of the
	 *      _scheduler (which is a buffer to remember the last scheduling
	 *      choice). These operations are:
	 *
	 *      C(s)  construct the context with ID 's' and insert it
	 *      D(s)  remove the context with ID 's' and destruct it
	 *      A(s)  set the context with ID 's' active
	 *      I(s)  set the context with ID 's' inactive
	 *      O(s)  do 'A(s)' and check that this will outdate the head
	 *      N(s)  do 'A(s)' and check that this won't outdate the head
	 *      Y     annotate that the current head wants to yield
	 *
	 * U(c,t,s,q) 
	 *           First update the head and time of the _scheduler according to
	 *           the new schedule and the fact that the head consumed a 
	 *           quantum of 'c'. Then, check the consumed time 't' for the
	 *           actual round and if the new head is the context with ID 's'
	 *           that has a quota of 'q'.
	 *
	 * doc  Documents the expected schedule for the point after the head
	 *      update in the corresponding line. First it lists all claims
	 *      via their ID followed by a ' (active) or a ° (inactive) and the
	 *      corresponding quota. So 5°120 is the inactive context 5 that
	 *      has a quota of 120 left for the current round. They are sorted
	 *      decurrent by their priorities and the priority bands are
	 *      separated by dashes. After the lowest priority band there is
	 *      an additional dash followed by the current state of the round-
	 *      robin scheduling that has no quota or priority. Only active
	 *      contexts are listed here and only the head is listed together
	 *      with its remaining quota. So 4'50 1 7 means that the active
	 *      context 4 is the round-robin head with quota 50 remaining and
	 *      that he is followed by the active contexts 1 and 7 both with a
	 *      fresh time-slice.
	 *
	 * The order of operations is the same as in the operative kernel so each
	 * line can be seen as one "kernel pass". If any check in a line fails,
	 * the test prematurely stops and prints out where and why it has stopped.
	 */

	/* first round - idle */
	_update_head_and_check( 10,  10, 0, 100, __LINE__); /* - */
	_update_head_and_check( 90, 100, 0, 100, __LINE__); /* - */
	_update_head_and_check(120, 200, 0, 100, __LINE__); /* - */
	_update_head_and_check(130, 300, 0, 100, __LINE__); /* - */
	_update_head_and_check(140, 400, 0, 100, __LINE__); /* - */
	_update_head_and_check(150, 500, 0, 100, __LINE__); /* - */
	_update_head_and_check(160, 600, 0, 100, __LINE__); /* - */
	_update_head_and_check(170, 700, 0, 100, __LINE__); /* - */
	_update_head_and_check(180, 800, 0, 100, __LINE__); /* - */
	_update_head_and_check(190, 900, 0, 100, __LINE__); /* - */
	_update_head_and_check(200,   0, 0, 100, __LINE__); /* - */


	/* second round - one claim, one filler */
	_construct_share(1, __LINE__);
	_update_head_and_check(111, 100, 0, 100, __LINE__); /* 1°230 - */

	A(1)
	_update_head_and_check(123, 200, 1, 230, __LINE__); /* 1'230 - 1'100 */

	I(1)
	_update_head_and_check(200, 400, 0, 100, __LINE__); /* 1°30 - */

	A(1)
	_update_head_and_check( 10, 410, 1,  30, __LINE__); /* 1'30 - 1'100 */
	_update_head_and_check(100, 440, 1, 100, __LINE__); /* 1'0 - 1'100 */
	_update_head_and_check(200, 540, 1, 100, __LINE__); /* 1'0 - 1'100 */

	I(1)
	_update_head_and_check(200, 640, 0, 100, __LINE__); /* 1°0 - */
	_update_head_and_check(200, 740, 0, 100, __LINE__); /* 1°0 - */

	A(1)
	_update_head_and_check( 10, 750, 1, 100, __LINE__); /* 1'0 - 1'100 */
	_update_head_and_check( 50, 800, 1,  50, __LINE__); /* 1'0 - 1'50 */
	_update_head_and_check( 20, 820, 1,  30, __LINE__); /* 1'0 - 1'30 */
	_update_head_and_check(100, 850, 1, 100, __LINE__); /* 1'0 - 1'100 */
	_update_head_and_check(200, 950, 1,  50, __LINE__); /* 1'0 - 1'100 */
	_update_head_and_check(200,   0, 1, 230, __LINE__); /* 1'230 - 1'100 */


	/* third round - one claim per priority */
	_construct_share(2, __LINE__);
	A(2)
	_update_head_and_check( 50,  50, 1, 180, __LINE__); /* 1'180 - 2'170 - 1'100 2 */

	I(1)
	_update_head_and_check( 70, 120, 2, 170, __LINE__); /* 1°110 - 2'170 - 2'100 */

	A(1) I(2)
	_update_head_and_check(110, 230, 1, 110, __LINE__); /* 1'110 - 2°60 - 1'100 */

	_update_head_and_check( 90, 320, 1,  20, __LINE__); /* 1'20 - 2°60 - 1'100 */

	A(2) I(1)
	_update_head_and_check( 10, 330, 2,  60, __LINE__); /* 1°10 - 2'60 - 2'100 */

	_construct_share(3, __LINE__);
	_update_head_and_check( 40, 370, 2,  20, __LINE__); /* 3°110 - 1°10 - 2'10 - 2'100 */

	A(3)
	_update_head_and_check( 10, 380, 3, 110, __LINE__); /* 3'110 - 1°10 - 2'10 - 2'100 3 */

	_update_head_and_check(150, 490, 2,  10, __LINE__); /* 3'0 - 1°10 - 2'10 - 2'100 3 */

	_update_head_and_check( 10, 500, 2, 100, __LINE__); /* 3'0 - 1°10 - 2'0 - 2'100 3 */

	_update_head_and_check( 60, 560, 2,  40, __LINE__); /* 3'0 - 1°10 - 2'0 - 2'40 3 */

	_construct_share(4, __LINE__);
	_update_head_and_check( 60, 600, 3, 100, __LINE__); /* 3'0 - 1°10 - 4°90 - 2'0 - 3'100 2 */

	_construct_share(6, __LINE__);
	A(6)
	_update_head_and_check(120, 700, 2, 100, __LINE__); /* 3'0 - 1°10 - 4°90 - 2'0 - 2'100 6 3*/

	A(4)
	_update_head_and_check( 80, 780, 4,  90, __LINE__); /* 3'0 - 1°10 - 4'90 - 2'0 - 2'20 6 3 4 */

	I(4) A(1)
	_update_head_and_check( 50, 830, 1,  10, __LINE__); /* 3'0 - 1'10 - 4°40 - 2'0 - 2'20 6 3 1 */

	_update_head_and_check( 50, 840, 2,  20, __LINE__); /* 3'0 - 1'0 - 4°40 - 2'0 - 2'20 6 3 1 */

	_update_head_and_check( 50, 860, 6, 100, __LINE__); /* 3'0 - 1'0 - 4°40 - 2'0 - 6'100 3 1 2 */

	_update_head_and_check(100, 960, 3,  40, __LINE__); /* 3'0 - 1'0 - 4°40 - 2'0 - 3'100 1 2 6 */

	_update_head_and_check( 60,   0, 3, 110, __LINE__); /* 3'110 - 1'230 - 4°40 - 2'170 - 3'60 1 2 6 */


	/* fourth round - multiple claims per priority */
	_construct_share(5, __LINE__);
	_update_head_and_check( 60,  60, 3,  50, __LINE__); /* 3'50 5°120 - 1'230 - 4°90 - 2'170 - 3'60 1 2 6 */

	A(4) I(3)
	_update_head_and_check( 40, 100, 1, 230, __LINE__); /* 3°10 5°120 - 1'230 - 4'90 - 2'170 - 1'100 2 6 4 */

	_construct_share(7, __LINE__);
	A(7)
	_update_head_and_check(200, 300, 7, 180, __LINE__); /* 3°10 5°120 - 7'180 1'30 - 4'90 - 2'170 - 1'100 2 6 4 7 */

	_construct_share(8, __LINE__);
	A(5)
	_update_head_and_check(100, 400, 5, 120, __LINE__); /* 5'120 3°10 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 */

	A(3)
	_update_head_and_check(100, 500, 3,  10, __LINE__); /* 3'10 5'20 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 3 */

	_update_head_and_check( 30, 510, 5,  20, __LINE__); /* 5'20 3'0 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 3 */

	_construct_share(9, __LINE__); A(9)
	_update_head_and_check( 10, 520, 5,  10, __LINE__); /* 5'10 3'0 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 3 9 */

	_update_head_and_check( 50, 530, 7,  80, __LINE__); /* 5'0 3'0 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 3 9 */

	A(8) I(7)
	_update_head_and_check( 10, 540, 8, 100, __LINE__); /* 5'0 3'0 - 8'100 1'30 7°70 - 4'90 - 2'170 - 1'100 2 6 4 5 3 9 8 */

	I(8)
	_update_head_and_check( 80, 620, 1,  30, __LINE__); /* 5'0 3'0 - 1'30 7°70 8°20 - 4'90 - 2'170 - 1'100 2 6 4 5 3 9 */

	_update_head_and_check(200, 650, 4,  90, __LINE__); /* 5'0 3'0 - 1'0 7°70 8°20 - 4'90 - 2'170 - 1'100 2 6 4 5 3 9 */

	_update_head_and_check(100, 740, 2, 170, __LINE__); /* 5'0 3'0 - 1'0 7°70 8°20 - 4'0 - 2'170 - 1'100 2 6 4 5 3 9 */

	A(8) A(7)
	_update_head_and_check( 10, 750, 7,  70, __LINE__); /* 5'0 3'0 - 7'70 8'20 1'0 - 4'0 - 2'160 - 1'100 2 6 4 5 3 9 8 7 */

	I(7) I(3)
	_update_head_and_check( 10, 760, 8,  20, __LINE__); /* 5'0 3°0 - 8'20 1'0 7°60 - 4'0 - 2'160 - 1'100 2 6 4 5 9 8 */

	I(8)
	_update_head_and_check( 10, 770, 2, 160, __LINE__); /* 5'0 3°0 - 1'0 7°60 8°10 - 4'0 - 2'160 - 1'100 2 6 4 5 9 */

	I(2)
	_update_head_and_check( 40, 810, 1, 100, __LINE__); /* 5'0 3°0 - 1'0 7°60 8°10 - 4'0 - 2°120 - 1'100 6 4 5 9 */

	A(3)
	_update_head_and_check( 30, 840, 1,  70, __LINE__); /* 5'0 3'0 - 1'0 7°60 8°10 - 4'0 - 2°120 - 1'100 6 4 5 9 3 */

	_update_head_and_check( 80, 910, 6,  90, __LINE__); /* 5'0 3'0 - 1'0 7°60 8°10 - 4'0 - 2°120 - 6'100 4 5 9 3 1 */

	A(7) A(8)
	_update_head_and_check( 10, 920, 8,  10, __LINE__); /* 5'0 3'0 - 8'10 7'60 1'0 - 4'0 - 2°120 - 6'90 4 5 9 3 1 7 8 */

	_update_head_and_check( 30, 930, 7,  60, __LINE__); /* 5'0 3'0 - 7'60 1'0 8'0 - 4°0 - 2°120 - 6'90 4 5 9 3 1 7 8 */

	A(2) I(7)
	_update_head_and_check( 10, 940, 2,  60, __LINE__); /* 5'0 3'0 - 1'0 8'0 7°50 - 4'0 - 2'120 - 6'90 4 5 9 3 1 8 2 */

	I(3) I(5)
	_update_head_and_check( 40, 980, 2,  20, __LINE__); /* 5°0 3°0 - 1'0 8'0 7°50 - 4'0 - 2'80 - 6'90 4 9 1 8 2 */

	I(9) I(4)
	_update_head_and_check( 10, 990, 2,  10, __LINE__); /* 5°0 3°0 - 1'0 8'0 7°50 - 4°0 - 2'70 - 6'90 1 8 2 */

	_update_head_and_check( 40,   0, 1, 230, __LINE__); /* 5°120 3°110 - 1'230 8'100 7°180 - 4°90 - 2'170 - 6'90 1 8 2 */


	/* fifth round - yield, ready & check*/
	I(6)
	_update_head_and_check( 30,  30, 1, 200, __LINE__); /* 5°120 3°110 - 1'200 8'100 7°180 - 4°90 - 2'170 - 1'100 8 2 */

	Y
	_update_head_and_check( 20,  50, 8, 100, __LINE__); /* 5°120 3°110 - 8'100 1'0 7°180 - 4°90 - 2'170 - 1'100 8 2 */

	_update_head_and_check(200, 150, 2, 170, __LINE__); /* 5°120 3°110 - 1'0 8'0 7°180 - 4°90 - 2'170 - 1'100 8 2 */

	Y
	_update_head_and_check( 70, 220, 1, 100, __LINE__); /* 5°120 3°110 - 1'0 8'0 7°180 - 4°90 - 2'0 - 1'100 8 2 */

	I(8) Y
	_update_head_and_check( 40, 260, 2, 100, __LINE__); /* 5°120 3°110 - 1'0 7°180 8°0 - 4°90 - 2'0 - 2'100 1 */

	I(1)
	_update_head_and_check( 50, 310, 2,  50, __LINE__); /* 5°120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'50 */

	_update_head_and_check( 10, 320, 2,  40, __LINE__); /* 5°120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'40 */


/* Runtime Error */
	N(1)
	_update_head_and_check(200, 360, 1, 100, __LINE__); /* 5°120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 1'100 2 */

	_update_head_and_check( 10, 370, 1,  90, __LINE__); /* 5°120 3°110 - 1'0 7°180 8°0 - 4°90 - 2'0 - 1'90 2 */

	I(1)
	_update_head_and_check( 10, 380, 2, 100, __LINE__); /* 5°120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'100 */

	O(5)
	_update_head_and_check( 10, 390, 5, 120, __LINE__); /* 5'120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'90 5 */

	Y
	_update_head_and_check( 90, 480, 2,  90, __LINE__); /* 5'0 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'90 5 */

	Y
	_update_head_and_check( 10, 490, 5, 100, __LINE__); /* 5'0 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 5'100 2 */

	O(7)
	_update_head_and_check( 10, 500, 7, 180, __LINE__); /* 5'0 3°110 - 7'180 1°0 8°0 - 4°90 - 2'0 - 5'90 2 7 */

	Y
	_update_head_and_check( 10, 510, 5,  90, __LINE__); /* 5'0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 5'90 2 7 */

	Y
	_update_head_and_check( 10, 520, 2, 100, __LINE__); /* 5'0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 2'100 7 5 */

	Y
	_update_head_and_check( 10, 530, 7, 100, __LINE__); /* 5'0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 7'100 5 2 */

	I(5)
	_update_head_and_check( 10, 540, 7,  90, __LINE__); /* 5°0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 7'90 2 */

	I(7) N(5)
	_update_head_and_check( 10, 550, 2, 100, __LINE__); /* 5'0 3°110 - 7°0 1°0 8°0 - 4°90 - 2'0 - 2'100 5 */

	N(7)
	_update_head_and_check(200, 650, 5, 100, __LINE__); /* 5'0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 5'100 7 2 */

	I(5) I(7)
	_update_head_and_check( 10, 660, 2, 100, __LINE__); /* 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2'0 - 2'100 */

	I(2)
	_update_head_and_check( 10, 670, 0, 100, __LINE__); /* 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - */

	_update_head_and_check( 10, 680, 0, 100, __LINE__); /* 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - */

	_update_head_and_check(100, 780, 0, 100, __LINE__); /* 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - */

	O(9)
	_update_head_and_check( 10, 790, 9, 100, __LINE__); /* 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - 9'100 */

	N(6)
	_update_head_and_check( 20, 810, 9,  80, __LINE__); /* 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - 9'80 6 */

	N(8)
	_update_head_and_check( 10, 820, 9,  70, __LINE__); /* 5°0 3°110 - 8'0 7°0 1°0 - 4°90 - 2°0 - 9'70 6 8 */

	Y
	_update_head_and_check( 10, 830, 6, 100, __LINE__); /* 5°0 3°110 - 8'0 7°0 1°0 - 4°90 - 2°0 - 6'100 8 9 */

	Y
	_update_head_and_check( 10, 840, 8, 100, __LINE__); /* 5°0 3°110 - 8'0 7°0 1°0 - 4°90 - 2°0 - 8'100 9 6 */

	N(7) Y
	_update_head_and_check( 20, 860, 9, 100, __LINE__); /* 5°0 3°110 - 8'0 7'0 1°0 - 4°90 - 2°0 - 9'100 6 7 8 */

	I(8) I(9)
	_update_head_and_check( 10, 870, 6, 100, __LINE__); /* 5°0 3°110 - 7'0 8°0 1°0 - 4°90 - 2°0 - 6'100 7 */

	I(6) I(7)
	_update_head_and_check( 10, 880, 0, 100, __LINE__); /* 5°0 3°110 - 7°0 8°0 1°0 - 4°90 - 2°0 - */

	O(4)
	_update_head_and_check( 20, 900, 4,  90, __LINE__); /* 5°0 3°110 - 7°0 8°0 1°0 - 4'90 - 2°0 - 4'100 */

	O(3) N(1)
	_update_head_and_check( 10, 910, 3,  90, __LINE__); /* 3'110 5°0 - 1'0 7°0 8°0 - 4'80 - 2°0 - 4'100 3 1 */

	N(5) I(4)
	_update_head_and_check( 10, 920, 3,  80, __LINE__); /* 3'100 5'0 - 1'0 7°0 8°0 - 4°80 - 2°0 - 3 1 5 */

	I(3)
	_update_head_and_check( 10, 930, 1,  70, __LINE__); /* 5'0 3°90 - 1'0 7°0 8°0 - 4°80 - 2°0 - 1'100 5 */

	O(3)
	_update_head_and_check( 10, 940, 3,  60, __LINE__); /* 3'90 5'0 - 1'0 7°0 8°0 - 4°80 - 2°0 - 1'90 5 3 */

	N(4)
	_update_head_and_check( 10, 950, 3,  50, __LINE__); /* 3'80 5'0 - 1'0 7°0 8°0 - 4'80 - 2°0 - 1'90 5 3 4 */

	I(4)
	_update_head_and_check( 10, 960, 3,  40, __LINE__); /* 3'70 5'0 - 1'0 7°0 8°0 - 4°80 - 2°0 - 1'90 5 3 */

	I(3) N(4)
	_update_head_and_check( 10, 970, 4,  30, __LINE__); /* 5'0 3°60 - 1'0 7°0 8°0 - 4'80 - 2°0 - 1'90 5 4 */

	I(4)
	_update_head_and_check( 10, 980, 1,  20, __LINE__); /* 5'0 3°60 - 1'0 7°0 8°0 - 4°70 - 2°0 - 1'90 5 */

	O(3) O(4)
	_update_head_and_check( 10, 990, 3,  10, __LINE__); /* 3'60 5'0 - 1'0 7°0 8°0 - 4'70 - 2°0 - 1'80 5 3 4 */

	Y
	_update_head_and_check( 10,   0, 5, 120, __LINE__); /* 5'120 3'110 - 1'230 7°180 8°100 - 4'90 - 2°170 - 1'80 5 3 4 */


	/* sixth round - destroy and re-create */
	D(3)
	_update_head_and_check( 30,  30, 5,  90, __LINE__); /* 5'90 - 1'230 7°180 8°100 - 4'90 - 2°170 - 1'80 5 4 */

	I(5)
	_update_head_and_check( 30,  60, 1, 230, __LINE__); /* 5°60 - 1'230 7°180 8°100 - 4'90 - 2°170 - 1'80 4 */

	D(4) D(7)
	_update_head_and_check( 20,  80, 1, 210, __LINE__); /* 5°60 - 1'210 8°100 - 2°170 - 1'80 4 */

	I(1) N(9)
	_update_head_and_check( 40, 120, 9, 100, __LINE__); /* 5°60 - 1°170 8°100 - 2°170 - 9'100 */

	A(5) O(8)
	_update_head_and_check( 70, 190, 5,  60, __LINE__); /* 5'60 - 1°170 8'100 - 2°170 - 9'30 5 8 */

	D(8) I(5)
	_update_head_and_check( 10, 200, 9,  30, __LINE__); /* 5°60 - 1°170 - 2°170 - 9'30 */

	N(6)
	_construct_share(4, __LINE__);
	_update_head_and_check( 10, 210, 9,  20, __LINE__); /* 5°60 - 1°170 - 4°90 - 2°170 - 9'20 6 */

	D(5)
	O(4)
	_update_head_and_check( 10, 220, 4,  90, __LINE__); /* 1°170 - 4'90 - 2°170 - 9'10 6 4 */

	_update_head_and_check(100, 310, 9,  10, __LINE__); /* 1°170 - 4'0 - 2°170 - 9'10 6 4 */

	_update_head_and_check( 10, 320, 6, 100, __LINE__); /* 1°170 - 4'0 - 2°170 - 6'100 4 9 */

	D(4)
	_update_head_and_check(200, 420, 9, 100, __LINE__); /* 1°170 - 2°170 - 9'100 6 */

	_construct_share(5, __LINE__);
	A(5)
	_update_head_and_check( 10, 430, 5, 120, __LINE__); /* 5'120 - 1°210 - 2°170 - 9'90 6 5 */

	_construct_share(4, __LINE__);
	Y
	_update_head_and_check( 10, 440, 9,  90, __LINE__); /* 5'0 - 1°170 - 4°90 - 2°170 - 9'90 6 5 */

	O(4) Y
	_update_head_and_check( 50, 490, 4,  90, __LINE__); /* 5'0 - 1°170 - 4'90 - 2°170 - 6'100 5 4 9 */

	D(6) Y
	_update_head_and_check( 10, 500, 5, 100, __LINE__); /* 5'0 - 1°170 - 4'0 - 2°170 - 5'100 4 9 */

	D(9)
	_update_head_and_check(200, 600, 4, 100, __LINE__); /* 5'0 - 1°170 - 4'0 - 2°170 - 4'100 5 */

	_construct_share(7, __LINE__); _construct_share(8, __LINE__);
	_update_head_and_check(200, 700, 5, 100, __LINE__); /* 5'0 - 1°170 7°180 8°100 - 4'0 - 2°170 - 5'100 4 */

	O(1) O(7)
	_update_head_and_check( 10, 710, 7, 180, __LINE__); /* 5'0 - 7'180 1'170 8°100 - 4'0 - 2°170 - 5'90 4 1 7 */

	O(8)
	_update_head_and_check( 40, 750, 8, 100, __LINE__); /* 5'0 - 8'100 7'140 1'170 - 4'0 - 2°170 - 5'90 4 1 7 8 */

	D(7)
	_update_head_and_check(200, 850, 1, 150, __LINE__); /* 5'0 - 1'170 8'0 - 4'0 - 2°170 - 5'90 4 1 8 */

	_update_head_and_check( 50, 900, 1, 100, __LINE__); /* 5'0 - 1'120 8'0 - 4'0 - 2°170 - 5'90 4 1 8 */

	Y
	_update_head_and_check( 60, 960, 5,  40, __LINE__); /* 5'0 - 8'0 1'0 - 4'0 - 2°170 - 5'90 4 1 8 */

	_update_head_and_check(100,   0, 5, 120, __LINE__); /* 5'120 - 8'100 1'230 - 4'90 - 2°170 - 5'50 4 1 8 */


	/* seventh round - re-configure quota, first part */
	Q(5, 100) _construct_share(6, __LINE__);
	_update_head_and_check( 40,  40, 5,  80, __LINE__); /* 5'80 - 8'100 1'230 - 4'90 - 2°170 - 5'50 4 1 8 */

	Q(5,  70) A(6)
	_update_head_and_check( 10,  50, 5,  70, __LINE__); /* 5'70 - 8'100 1'230 - 4'90 - 2°170 - 5'50 4 1 8 6 */

	Q(5,  40) _construct_share(9, __LINE__);
	_update_head_and_check( 10,  60, 5,  40, __LINE__); /* 5'40 - 8'100 1'230 - 4'90 - 2°170 - 5'50 4 1 8 6 */

	Q(5,   0) A(9)
	_update_head_and_check( 20,  80, 8, 100, __LINE__); /* 8'100 1'230 - 4'90 - 2°170 - 5'50 4 1 8 6 9 */

	Q(8, 120)
	_update_head_and_check( 30, 110, 8,  70, __LINE__); /* 8'70 1'230 - 4'90 - 2°170 - 5'50 4 1 8 6 9 */

	Q(9,  40)
	_update_head_and_check( 10, 120, 8,  60, __LINE__); /* 8'60 1'230 9'0 - 4'90 - 2°170 - 5'50 4 1 8 6 9 */

	Q(8, 130)
	_update_head_and_check( 10, 130, 8,  50, __LINE__); /* 8'50 1'230 9'0 - 4'90 - 2°170 - 5'50 4 1 8 6 9 */

	Q(8,  50)
	_update_head_and_check( 20, 150, 8,  30, __LINE__); /* 8'30 1'230 9'0 - 4'90 - 2°170 - 5'50 4 1 8 6 9 */

	Q(6,  60)
	_update_head_and_check( 10, 160, 8,  20, __LINE__); /* 6'0 - 8'20 1'230 9'0 - 4'90 - 2°170 - 5'50 4 1 8 6 9 */

	I(8)
	_update_head_and_check( 10, 170, 1, 230, __LINE__); /* 6'0 - 1'230 9'0 8°10 - 4'90 - 2°170 - 5'50 4 1 6 9 */

	I(1)
	_update_head_and_check(100, 270, 4,  90, __LINE__); /* 6'0 - 9'0 8°10 1°130 - 4'90 - 2°170 - 5'50 4 6 9 */

	_update_head_and_check(100, 360, 5,  50, __LINE__); /* 6'0 - 9'0 8°10 1°130 - 4'0 - 2°170 - 5'50 4 6 9 */

	Q(1, 110)
	_update_head_and_check( 10, 370, 5,  40, __LINE__); /* 6'0 - 9'0 8°10 1°110 - 4'0 - 2°170 - 5'40 4 6 9 */

	Q(1, 120)
	_update_head_and_check( 20, 390, 5,  20, __LINE__); /* 6'0 - 9'0 8°10 1°110 - 4'0 - 2°170 - 5'20 4 6 9 */

	Q(4, 210)
	_update_head_and_check( 10, 400, 5,  10, __LINE__); /* 6'0 - 9'0 8°10 1°110 - 4'0 - 2°170 - 5'10 4 6 9 */

	A(1)
	_update_head_and_check( 20, 410, 1, 110, __LINE__); /* 6'0 - 1'110 9'0 8°10 - 4'0 - 2°170 - 4'100 6 9 1 5 */

	Q(1, 100)
	_update_head_and_check( 30, 440, 1,  80, __LINE__); /* 6'0 - 1'80 9'0 8°10 - 4'0 - 2°170 - 4'100 6 9 1 5 */

	O(8)
	_update_head_and_check( 10, 450, 8,  10, __LINE__); /* 6'0 - 8'10 1'70 9'0 - 4'0 - 2°170 - 4'100 6 9 1 5 8 */

	N(2)
	_update_head_and_check( 10, 460, 1,  70, __LINE__); /* 6'0 - 1'70 9'0 8'0 - 4'0 - 2'170 - 4'100 6 9 1 5 8 2 */

	Q(1,  20)
	_update_head_and_check( 30, 490, 1,  20, __LINE__); /* 6'0 - 1'20 9'0 8'0 - 4'0 - 2'170 - 4'100 6 9 1 5 8 2 */

	Q(1,  50)
	_update_head_and_check( 10, 500, 1,  10, __LINE__); /* 6'0 - 1'10 9'0 8'0 - 4'0 - 2'170 - 4'100 6 9 1 5 8 2 */

	Q(1,   0)
	_update_head_and_check( 30, 510, 2, 170, __LINE__); /* 6'0 - 9'0 8'0 - 4'0 - 2'170 - 4'100 6 9 1 5 8 2 */

	Q(2, 180)
	_update_head_and_check( 50, 560, 2, 120, __LINE__); /* 6'0 - 9'0 8'0 - 4'0 - 2'120 - 4'100 6 9 1 5 8 2 */

	I(2) Q(4,  80)
	_update_head_and_check( 70, 630, 4, 100, __LINE__); /* 6'0 - 9'0 8'0 - 4'0 - 2°50 - 4'100 6 9 1 5 8 */

	_update_head_and_check( 50, 680, 4,  50, __LINE__); /* 6'0 - 9'0 8'0 - 4'0 - 2°50 - 4'100 6 9 1 5 8 */

	I(4)
	_update_head_and_check( 10, 690, 6, 100, __LINE__); /* 6'0 - 9'0 8'0 - 4°0 - 2°50 - 6'100 9 1 5 8 */

	_construct_share(3, __LINE__);
	_update_head_and_check( 30, 720, 6,  70, __LINE__); /* 6'0 3°110 - 9'0 8'0 - 4°0 - 2°50 - 6'70 9 1 5 8 */

	Q(3, 210) Y
	_update_head_and_check( 20, 740, 9, 100, __LINE__); /* 6'0 3°110 - 9'0 8'0 - 4°0 - 2°50 - 9'100 1 5 8 6 */

	I(9) N(4)
	_update_head_and_check( 50, 790, 1, 100, __LINE__); /* 6'0 3°110 - 9°0 8'0 - 4'0 - 2°50 - 1'100 5 8 6 4 */

	O(2)
	_update_head_and_check(170, 890, 2,  50, __LINE__); /* 6'0 3°110 - 9°0 8'0 - 4'0 - 2'50 - 5'100 8 6 4 2 1 */

	Y N(9)
	_update_head_and_check( 60, 940, 5,  60, __LINE__); /* 6'0 3°110 - 9'0 8'0 - 4'0 - 2'0 - 5'100 8 6 4 2 1 9 */

	I(6)
	_update_head_and_check( 20, 960, 5,  40, __LINE__); /* 6°0 3°110 - 9'0 8'0 - 4'0 - 2'0 - 5'80 8 4 2 1 9 */

	Y
	_update_head_and_check( 10, 970, 8,  30, __LINE__); /* 6°0 3°110 - 9'0 8'0 - 4'0 - 2'0 - 8'100 4 2 1 9 5 */

	I(8) Y
	_update_head_and_check( 10, 980, 4,  20, __LINE__); /* 6°0 3°110 - 9'0 8°0 - 4'0 - 2'0 - 4'100 2 1 9 5 */

	Y
	_update_head_and_check( 20,   0, 9,  40, __LINE__); /* 6°60 3°210 - 9'40 8°50 - 4'80 - 2'180 - 2'100 1 9 5 4 */


	/* eighth round - re-configure quota, second part */
	A(3)
	_update_head_and_check( 30,  30, 3, 210, __LINE__); /* 3'210 6°60 - 9'10 8°50 - 4'80 - 2'180 - 2'100 1 9 5 4 3 */

	I(3)
	_update_head_and_check(110, 140, 9,  10, __LINE__); /* 6°60 3°100 - 9'10 8°50 - 4'80 - 2'180 - 2'100 1 9 5 4 */

	_update_head_and_check( 40, 150, 4,  80, __LINE__); /* 6°60 3°100 - 9'0 8°50 - 4'80 - 2'180 - 2'100 1 9 5 4 */

	_update_head_and_check(100, 230, 2, 180, __LINE__); /* 6°60 3°100 - 9'0 8°50 - 4'0 - 2'180 - 2'100 1 9 5 4 */

	Q(2, 90)
	_update_head_and_check( 40, 270, 2,  90, __LINE__); /* 6°60 3°100 - 9'0 8°50 - 4'0 - 2'90 - 2'100 1 9 5 4 */

	Y Q(8, 130)
	_update_head_and_check( 40, 310, 2, 100, __LINE__); /* 6°60 3°100 - 9'0 8°50 - 4'0 - 2'0 - 2'100 1 9 5 4 */

	_update_head_and_check(100, 410, 1, 100, __LINE__); /* 6°60 3°100 - 9'0 8°50 - 4'0 - 2'0 - 1'100 9 5 4 2 */

	Q(3, 80) O(3)
	_update_head_and_check( 60, 470, 3,  80, __LINE__); /* 3'80 6°60 - 9'0 8°50 - 4'0 - 2'0 - 1'40 9 5 4 2 3*/

	N(8) Q(8, 50)
	_update_head_and_check(100, 550, 8,  50, __LINE__); /* 3'0 6°60 - 8'50 9'0 - 4'0 - 2'0 - 1'40 9 5 4 2 3 8 */

	_update_head_and_check( 20, 570, 8,  30, __LINE__); /* 3'0 6°60 - 8'30 9'0 - 4'0 - 2'0 - 1'40 9 5 4 2 3 8 */

	O(6) Q(6, 50)
	_update_head_and_check( 10, 580, 6,  50, __LINE__); /* 6'50 3'0 - 8'20 9'0 - 4'0 - 2'0 - 1'40 9 5 4 2 3 8 6 */

	_update_head_and_check( 70, 630, 8,  20, __LINE__); /* 6'0 3'0 - 9'0 8'20 - 4'0 - 2'0 - 1'40 9 5 4 2 3 8 6 */

	_update_head_and_check( 40, 650, 1,  40, __LINE__); /* 6'0 3'0 - 9'0 8'0 - 4'0 - 2'0 - 1'40 9 5 4 2 3 8 6 */

	_update_head_and_check( 40, 690, 9, 100, __LINE__); /* 6'0 3'0 - 9'0 8'0 - 4'0 - 2'0 - 9'100 5 4 2 3 8 6 1 */

	D(6)
	_update_head_and_check(200, 790, 5, 100, __LINE__); /* 3'0 - 9'0 8'0 - 4'0 - 2'0 - 5'100 4 2 3 8 1 9 */

	D(3)
	_update_head_and_check(100, 890, 4, 100, __LINE__); /* 9'0 8'0 - 4'0 - 2'0 - 4'100 2 8 1 9 5 */

	_update_head_and_check(120, 990, 2,  10, __LINE__); /* 9'0 8'0 - 4'0 - 2'0 - 2'100 8 1 9 5 4 */

	_update_head_and_check( 80,   0, 9,  40, __LINE__); /* 9'40 8'50 - 4'80 - 2'90 - 2'90 8 1 9 5 4 */


	_env.parent().exit(0);
}


void Component::construct(Env &env)
{
	static Cpu_scheduler_test::Main main { env };
}
