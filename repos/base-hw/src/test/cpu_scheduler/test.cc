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
		Cpu_share                 _idle_share               { 0, 0 };
		Constructible<Cpu_share>  _shares[MAX_NR_OF_SHARES] { };
		time_t                    _current_time             { 0 };
		Cpu_scheduler             _scheduler;

		unsigned _id_of_share(Cpu_share const &share) const
		{
			if (&share == &_idle_share) {

				return IDLE_SHARE_ID;
			}
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

		Cpu_share &_share(unsigned const id)
		{
			if (id == IDLE_SHARE_ID) {

				return _idle_share;
			}
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
			_shares[id].construct(prio, quota, id);
			_scheduler.insert(*_shares[id]);
		}

		void _destroy_share(unsigned const id,
		                    unsigned const line_nr)
		{
			if (id == IDLE_SHARE_ID ||
			    id >= sizeof(_shares)/sizeof(_shares[0])) {

				error("failed to destroy share in line ", line_nr);
				_env.parent().exit(-1);
			}
			if (!_shares[id].constructed()) {

				error("failed to destroy share in line ", line_nr);
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

		void _set_share_ready_and_check(unsigned const share_id,
		                                bool     const expect_head_outdated,
		                                unsigned const line_nr)
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


Cpu_scheduler_test::Main::Main(Env &env)
:
	_env       { env },
	_scheduler { _idle_share, 1000, 100 }
{
	/********************
	 ** Round #1: Idle **
	 ********************/

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


	/*************************************
	 ** Round #2: One claim, one filler **
	 *************************************/

	_construct_share(1, __LINE__);
	_update_head_and_check(111, 100, 0, 100, __LINE__); /* 1°230 - */

	_scheduler.ready(_share(1));
	_update_head_and_check(123, 200, 1, 230, __LINE__); /* 1'230 - 1'100 */

	_scheduler.unready(_share(1));
	_update_head_and_check(200, 400, 0, 100, __LINE__); /* 1°30 - */

	_scheduler.ready(_share(1));
	_update_head_and_check( 10, 410, 1,  30, __LINE__); /* 1'30 - 1'100 */
	_update_head_and_check(100, 440, 1, 100, __LINE__); /* 1'0 - 1'100 */
	_update_head_and_check(200, 540, 1, 100, __LINE__); /* 1'0 - 1'100 */

	_scheduler.unready(_share(1));
	_update_head_and_check(200, 640, 0, 100, __LINE__); /* 1°0 - */
	_update_head_and_check(200, 740, 0, 100, __LINE__); /* 1°0 - */

	_scheduler.ready(_share(1));
	_update_head_and_check( 10, 750, 1, 100, __LINE__); /* 1'0 - 1'100 */
	_update_head_and_check( 50, 800, 1,  50, __LINE__); /* 1'0 - 1'50 */
	_update_head_and_check( 20, 820, 1,  30, __LINE__); /* 1'0 - 1'30 */
	_update_head_and_check(100, 850, 1, 100, __LINE__); /* 1'0 - 1'100 */
	_update_head_and_check(200, 950, 1,  50, __LINE__); /* 1'0 - 1'100 */
	_update_head_and_check(200,   0, 1, 230, __LINE__); /* 1'230 - 1'100 */


	/**************************************
	 ** Round #3: One claim per priority **
	 **************************************/

	_construct_share(2, __LINE__);
	_scheduler.ready(_share(2));
	_update_head_and_check( 50,  50, 1, 180, __LINE__); /* 1'180 - 2'170 - 1'100 2 */

	_scheduler.unready(_share(1));
	_update_head_and_check( 70, 120, 2, 170, __LINE__); /* 1°110 - 2'170 - 2'100 */

	_scheduler.ready(_share(1));
	_scheduler.unready(_share(2));
	_update_head_and_check(110, 230, 1, 110, __LINE__); /* 1'110 - 2°60 - 1'100 */
	_update_head_and_check( 90, 320, 1,  20, __LINE__); /* 1'20 - 2°60 - 1'100 */

	_scheduler.ready(_share(2));
	_scheduler.unready(_share(1));
	_update_head_and_check( 10, 330, 2,  60, __LINE__); /* 1°10 - 2'60 - 2'100 */

	_construct_share(3, __LINE__);
	_update_head_and_check( 40, 370, 2,  20, __LINE__); /* 3°110 - 1°10 - 2'10 - 2'100 */

	_scheduler.ready(_share(3));
	_update_head_and_check( 10, 380, 3, 110, __LINE__); /* 3'110 - 1°10 - 2'10 - 2'100 3 */
	_update_head_and_check(150, 490, 2,  10, __LINE__); /* 3'0 - 1°10 - 2'10 - 2'100 3 */
	_update_head_and_check( 10, 500, 2, 100, __LINE__); /* 3'0 - 1°10 - 2'0 - 2'100 3 */
	_update_head_and_check( 60, 560, 2,  40, __LINE__); /* 3'0 - 1°10 - 2'0 - 2'40 3 */

	_construct_share(4, __LINE__);
	_update_head_and_check( 60, 600, 3, 100, __LINE__); /* 3'0 - 1°10 - 4°90 - 2'0 - 3'100 2 */

	_construct_share(6, __LINE__);
	_scheduler.ready(_share(6));
	_update_head_and_check(120, 700, 2, 100, __LINE__); /* 3'0 - 1°10 - 4°90 - 2'0 - 2'100 6 3*/

	_scheduler.ready(_share(4));
	_update_head_and_check( 80, 780, 4,  90, __LINE__); /* 3'0 - 1°10 - 4'90 - 2'0 - 2'20 6 3 4 */

	_scheduler.unready(_share(4));
	_scheduler.ready(_share(1));
	_update_head_and_check( 50, 830, 1,  10, __LINE__); /* 3'0 - 1'10 - 4°40 - 2'0 - 2'20 6 3 1 */
	_update_head_and_check( 50, 840, 2,  20, __LINE__); /* 3'0 - 1'0 - 4°40 - 2'0 - 2'20 6 3 1 */
	_update_head_and_check( 50, 860, 6, 100, __LINE__); /* 3'0 - 1'0 - 4°40 - 2'0 - 6'100 3 1 2 */
	_update_head_and_check(100, 960, 3,  40, __LINE__); /* 3'0 - 1'0 - 4°40 - 2'0 - 3'100 1 2 6 */
	_update_head_and_check( 60,   0, 3, 110, __LINE__); /* 3'110 - 1'230 - 4°40 - 2'170 - 3'60 1 2 6 */


	/********************************************
	 ** Round #4: Multiple claims per priority **
	 ********************************************/

	_construct_share(5, __LINE__);
	_update_head_and_check( 60,  60, 3,  50, __LINE__); /* 3'50 5°120 - 1'230 - 4°90 - 2'170 - 3'60 1 2 6 */

	_scheduler.ready(_share(4));
	_scheduler.unready(_share(3));
	_update_head_and_check( 40, 100, 1, 230, __LINE__); /* 3°10 5°120 - 1'230 - 4'90 - 2'170 - 1'100 2 6 4 */

	_construct_share(7, __LINE__);
	_scheduler.ready(_share(7));
	_update_head_and_check(200, 300, 7, 180, __LINE__); /* 3°10 5°120 - 7'180 1'30 - 4'90 - 2'170 - 1'100 2 6 4 7 */

	_construct_share(8, __LINE__);
	_scheduler.ready(_share(5));
	_update_head_and_check(100, 400, 5, 120, __LINE__); /* 5'120 3°10 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 */

	_scheduler.ready(_share(3));
	_update_head_and_check(100, 500, 3,  10, __LINE__); /* 3'10 5'20 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 3 */
	_update_head_and_check( 30, 510, 5,  20, __LINE__); /* 5'20 3'0 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 3 */

	_construct_share(9, __LINE__);
	_scheduler.ready(_share(9));
	_update_head_and_check( 10, 520, 5,  10, __LINE__); /* 5'10 3'0 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 3 9 */
	_update_head_and_check( 50, 530, 7,  80, __LINE__); /* 5'0 3'0 - 7'80 1'30 8°100 - 4'90 - 2'170 - 1'100 2 6 4 7 5 3 9 */

	_scheduler.ready(_share(8));
	_scheduler.unready(_share(7));
	_update_head_and_check( 10, 540, 8, 100, __LINE__); /* 5'0 3'0 - 8'100 1'30 7°70 - 4'90 - 2'170 - 1'100 2 6 4 5 3 9 8 */

	_scheduler.unready(_share(8));
	_update_head_and_check( 80, 620, 1,  30, __LINE__); /* 5'0 3'0 - 1'30 7°70 8°20 - 4'90 - 2'170 - 1'100 2 6 4 5 3 9 */
	_update_head_and_check(200, 650, 4,  90, __LINE__); /* 5'0 3'0 - 1'0 7°70 8°20 - 4'90 - 2'170 - 1'100 2 6 4 5 3 9 */
	_update_head_and_check(100, 740, 2, 170, __LINE__); /* 5'0 3'0 - 1'0 7°70 8°20 - 4'0 - 2'170 - 1'100 2 6 4 5 3 9 */

	_scheduler.ready(_share(8));
	_scheduler.ready(_share(7));
	_update_head_and_check( 10, 750, 7,  70, __LINE__); /* 5'0 3'0 - 7'70 8'20 1'0 - 4'0 - 2'160 - 1'100 2 6 4 5 3 9 8 7 */

	_scheduler.unready(_share(7));
	_scheduler.unready(_share(3));
	_update_head_and_check( 10, 760, 8,  20, __LINE__); /* 5'0 3°0 - 8'20 1'0 7°60 - 4'0 - 2'160 - 1'100 2 6 4 5 9 8 */

	_scheduler.unready(_share(8));
	_update_head_and_check( 10, 770, 2, 160, __LINE__); /* 5'0 3°0 - 1'0 7°60 8°10 - 4'0 - 2'160 - 1'100 2 6 4 5 9 */

	_scheduler.unready(_share(2));
	_update_head_and_check( 40, 810, 1, 100, __LINE__); /* 5'0 3°0 - 1'0 7°60 8°10 - 4'0 - 2°120 - 1'100 6 4 5 9 */

	_scheduler.ready(_share(3));
	_update_head_and_check( 30, 840, 1,  70, __LINE__); /* 5'0 3'0 - 1'0 7°60 8°10 - 4'0 - 2°120 - 1'100 6 4 5 9 3 */
	_update_head_and_check( 80, 910, 6,  90, __LINE__); /* 5'0 3'0 - 1'0 7°60 8°10 - 4'0 - 2°120 - 6'100 4 5 9 3 1 */

	_scheduler.ready(_share(7));
	_scheduler.ready(_share(8));
	_update_head_and_check( 10, 920, 8,  10, __LINE__); /* 5'0 3'0 - 8'10 7'60 1'0 - 4'0 - 2°120 - 6'90 4 5 9 3 1 7 8 */
	_update_head_and_check( 30, 930, 7,  60, __LINE__); /* 5'0 3'0 - 7'60 1'0 8'0 - 4°0 - 2°120 - 6'90 4 5 9 3 1 7 8 */

	_scheduler.ready(_share(2));
	_scheduler.unready(_share(7));
	_update_head_and_check( 10, 940, 2,  60, __LINE__); /* 5'0 3'0 - 1'0 8'0 7°50 - 4'0 - 2'120 - 6'90 4 5 9 3 1 8 2 */

	_scheduler.unready(_share(3));
	_scheduler.unready(_share(5));
	_update_head_and_check( 40, 980, 2,  20, __LINE__); /* 5°0 3°0 - 1'0 8'0 7°50 - 4'0 - 2'80 - 6'90 4 9 1 8 2 */

	_scheduler.unready(_share(9));
	_scheduler.unready(_share(4));
	_update_head_and_check( 10, 990, 2,  10, __LINE__); /* 5°0 3°0 - 1'0 8'0 7°50 - 4°0 - 2'70 - 6'90 1 8 2 */
	_update_head_and_check( 40,   0, 1, 230, __LINE__); /* 5°120 3°110 - 1'230 8'100 7°180 - 4°90 - 2'170 - 6'90 1 8 2 */


	/************************************
	 ** Round #5: Yield, ready & check **
	 ************************************/

	_scheduler.unready(_share(6));
	_update_head_and_check( 30,  30, 1, 200, __LINE__); /* 5°120 3°110 - 1'200 8'100 7°180 - 4°90 - 2'170 - 1'100 8 2 */

	_scheduler.yield();
	_update_head_and_check( 20,  50, 8, 100, __LINE__); /* 5°120 3°110 - 8'100 1'0 7°180 - 4°90 - 2'170 - 1'100 8 2 */
	_update_head_and_check(200, 150, 2, 170, __LINE__); /* 5°120 3°110 - 1'0 8'0 7°180 - 4°90 - 2'170 - 1'100 8 2 */

	_scheduler.yield();
	_update_head_and_check( 70, 220, 1, 100, __LINE__); /* 5°120 3°110 - 1'0 8'0 7°180 - 4°90 - 2'0 - 1'100 8 2 */

	_scheduler.unready(_share(8));
	_scheduler.yield();
	_update_head_and_check( 40, 260, 2, 100, __LINE__); /* 5°120 3°110 - 1'0 7°180 8°0 - 4°90 - 2'0 - 2'100 1 */

	_scheduler.unready(_share(1));
	_update_head_and_check( 50, 310, 2,  50, __LINE__); /* 5°120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'50 */
	_update_head_and_check( 10, 320, 2,  40, __LINE__); /* 5°120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'40 */

	_set_share_ready_and_check(1, false, __LINE__);
	_update_head_and_check(200, 360, 1, 100, __LINE__); /* 5°120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 1'100 2 */
	_update_head_and_check( 10, 370, 1,  90, __LINE__); /* 5°120 3°110 - 1'0 7°180 8°0 - 4°90 - 2'0 - 1'90 2 */

	_scheduler.unready(_share(1));
	_update_head_and_check( 10, 380, 2, 100, __LINE__); /* 5°120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'100 */

	_set_share_ready_and_check(5, true, __LINE__);
	_update_head_and_check( 10, 390, 5, 120, __LINE__); /* 5'120 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'90 5 */

	_scheduler.yield();
	_update_head_and_check( 90, 480, 2,  90, __LINE__); /* 5'0 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 2'90 5 */

	_scheduler.yield();
	_update_head_and_check( 10, 490, 5, 100, __LINE__); /* 5'0 3°110 - 1°0 7°180 8°0 - 4°90 - 2'0 - 5'100 2 */

	_set_share_ready_and_check(7, true, __LINE__);
	_update_head_and_check( 10, 500, 7, 180, __LINE__); /* 5'0 3°110 - 7'180 1°0 8°0 - 4°90 - 2'0 - 5'90 2 7 */

	_scheduler.yield();
	_update_head_and_check( 10, 510, 5,  90, __LINE__); /* 5'0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 5'90 2 7 */

	_scheduler.yield();
	_update_head_and_check( 10, 520, 2, 100, __LINE__); /* 5'0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 2'100 7 5 */

	_scheduler.yield();
	_update_head_and_check( 10, 530, 7, 100, __LINE__); /* 5'0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 7'100 5 2 */

	_scheduler.unready(_share(5));
	_update_head_and_check( 10, 540, 7,  90, __LINE__); /* 5°0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 7'90 2 */

	_scheduler.unready(_share(7));
	_set_share_ready_and_check(5, false, __LINE__);
	_update_head_and_check( 10, 550, 2, 100, __LINE__); /* 5'0 3°110 - 7°0 1°0 8°0 - 4°90 - 2'0 - 2'100 5 */

	_set_share_ready_and_check(7, false, __LINE__);
	_update_head_and_check(200, 650, 5, 100, __LINE__); /* 5'0 3°110 - 7'0 1°0 8°0 - 4°90 - 2'0 - 5'100 7 2 */

	_scheduler.unready(_share(5));
	_scheduler.unready(_share(7));
	_update_head_and_check( 10, 660, 2, 100, __LINE__); /* 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2'0 - 2'100 */

	_scheduler.unready(_share(2));
	_update_head_and_check( 10, 670, 0, 100, __LINE__); /* 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - */
	_update_head_and_check( 10, 680, 0, 100, __LINE__); /* 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - */
	_update_head_and_check(100, 780, 0, 100, __LINE__); /* 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - */

	_set_share_ready_and_check(9, true, __LINE__);
	_update_head_and_check( 10, 790, 9, 100, __LINE__); /* 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - 9'100 */

	_set_share_ready_and_check(6, false, __LINE__);
	_update_head_and_check( 20, 810, 9,  80, __LINE__); /* 5°0 3°110 - 7°0 1°0 8°0 - 4°90 - 2°0 - 9'80 6 */

	_set_share_ready_and_check(8, false, __LINE__);
	_update_head_and_check( 10, 820, 9,  70, __LINE__); /* 5°0 3°110 - 8'0 7°0 1°0 - 4°90 - 2°0 - 9'70 6 8 */

	_scheduler.yield();
	_update_head_and_check( 10, 830, 6, 100, __LINE__); /* 5°0 3°110 - 8'0 7°0 1°0 - 4°90 - 2°0 - 6'100 8 9 */

	_scheduler.yield();
	_update_head_and_check( 10, 840, 8, 100, __LINE__); /* 5°0 3°110 - 8'0 7°0 1°0 - 4°90 - 2°0 - 8'100 9 6 */

	_set_share_ready_and_check(7, false, __LINE__);
	_scheduler.yield();
	_update_head_and_check( 20, 860, 9, 100, __LINE__); /* 5°0 3°110 - 8'0 7'0 1°0 - 4°90 - 2°0 - 9'100 6 7 8 */

	_scheduler.unready(_share(8));
	_scheduler.unready(_share(9));
	_update_head_and_check( 10, 870, 6, 100, __LINE__); /* 5°0 3°110 - 7'0 8°0 1°0 - 4°90 - 2°0 - 6'100 7 */

	_scheduler.unready(_share(6));
	_scheduler.unready(_share(7));
	_update_head_and_check( 10, 880, 0, 100, __LINE__); /* 5°0 3°110 - 7°0 8°0 1°0 - 4°90 - 2°0 - */

	_set_share_ready_and_check(4, true, __LINE__);
	_update_head_and_check( 20, 900, 4,  90, __LINE__); /* 5°0 3°110 - 7°0 8°0 1°0 - 4'90 - 2°0 - 4'100 */

	_set_share_ready_and_check(3, true, __LINE__);
	_set_share_ready_and_check(1, false, __LINE__);
	_update_head_and_check( 10, 910, 3,  90, __LINE__); /* 3'110 5°0 - 1'0 7°0 8°0 - 4'80 - 2°0 - 4'100 3 1 */

	_set_share_ready_and_check(5, false, __LINE__);
	_scheduler.unready(_share(4));
	_update_head_and_check( 10, 920, 3,  80, __LINE__); /* 3'100 5'0 - 1'0 7°0 8°0 - 4°80 - 2°0 - 3 1 5 */

	_scheduler.unready(_share(3));
	_update_head_and_check( 10, 930, 1,  70, __LINE__); /* 5'0 3°90 - 1'0 7°0 8°0 - 4°80 - 2°0 - 1'100 5 */

	_set_share_ready_and_check(3, true, __LINE__);
	_update_head_and_check( 10, 940, 3,  60, __LINE__); /* 3'90 5'0 - 1'0 7°0 8°0 - 4°80 - 2°0 - 1'90 5 3 */

	_set_share_ready_and_check(4, false, __LINE__);
	_update_head_and_check( 10, 950, 3,  50, __LINE__); /* 3'80 5'0 - 1'0 7°0 8°0 - 4'80 - 2°0 - 1'90 5 3 4 */

	_scheduler.unready(_share(4));
	_update_head_and_check( 10, 960, 3,  40, __LINE__); /* 3'70 5'0 - 1'0 7°0 8°0 - 4°80 - 2°0 - 1'90 5 3 */

	_scheduler.unready(_share(3));
	_set_share_ready_and_check(4, false, __LINE__);
	_update_head_and_check( 10, 970, 4,  30, __LINE__); /* 5'0 3°60 - 1'0 7°0 8°0 - 4'80 - 2°0 - 1'90 5 4 */

	_scheduler.unready(_share(4));
	_update_head_and_check( 10, 980, 1,  20, __LINE__); /* 5'0 3°60 - 1'0 7°0 8°0 - 4°70 - 2°0 - 1'90 5 */

	_set_share_ready_and_check(3, true, __LINE__);
	_set_share_ready_and_check(4, true, __LINE__);
	_update_head_and_check( 10, 990, 3,  10, __LINE__); /* 3'60 5'0 - 1'0 7°0 8°0 - 4'70 - 2°0 - 1'80 5 3 4 */

	_scheduler.yield();
	_update_head_and_check( 10,   0, 5, 120, __LINE__); /* 5'120 3'110 - 1'230 7°180 8°100 - 4'90 - 2°170 - 1'80 5 3 4 */


	/*************************************
	 ** Round #6: Destroy and re-create **
	 *************************************/

	_destroy_share(3, __LINE__);
	_update_head_and_check( 30,  30, 5,  90, __LINE__); /* 5'90 - 1'230 7°180 8°100 - 4'90 - 2°170 - 1'80 5 4 */

	_scheduler.unready(_share(5));
	_update_head_and_check( 30,  60, 1, 230, __LINE__); /* 5°60 - 1'230 7°180 8°100 - 4'90 - 2°170 - 1'80 4 */

	_destroy_share(4, __LINE__);
	_destroy_share(7, __LINE__);
	_update_head_and_check( 20,  80, 1, 210, __LINE__); /* 5°60 - 1'210 8°100 - 2°170 - 1'80 4 */

	_scheduler.unready(_share(1));
	_set_share_ready_and_check(9, false, __LINE__);
	_update_head_and_check( 40, 120, 9, 100, __LINE__); /* 5°60 - 1°170 8°100 - 2°170 - 9'100 */

	_scheduler.ready(_share(5));
	_set_share_ready_and_check(8, true, __LINE__);
	_update_head_and_check( 70, 190, 5,  60, __LINE__); /* 5'60 - 1°170 8'100 - 2°170 - 9'30 5 8 */

	_destroy_share(8, __LINE__);
	_scheduler.unready(_share(5));
	_update_head_and_check( 10, 200, 9,  30, __LINE__); /* 5°60 - 1°170 - 2°170 - 9'30 */

	_set_share_ready_and_check(6, false, __LINE__);
	_construct_share(4, __LINE__);
	_update_head_and_check( 10, 210, 9,  20, __LINE__); /* 5°60 - 1°170 - 4°90 - 2°170 - 9'20 6 */

	_destroy_share(5, __LINE__);
	_set_share_ready_and_check(4, true, __LINE__);
	_update_head_and_check( 10, 220, 4,  90, __LINE__); /* 1°170 - 4'90 - 2°170 - 9'10 6 4 */
	_update_head_and_check(100, 310, 9,  10, __LINE__); /* 1°170 - 4'0 - 2°170 - 9'10 6 4 */
	_update_head_and_check( 10, 320, 6, 100, __LINE__); /* 1°170 - 4'0 - 2°170 - 6'100 4 9 */

	_destroy_share(4, __LINE__);
	_update_head_and_check(200, 420, 9, 100, __LINE__); /* 1°170 - 2°170 - 9'100 6 */

	_construct_share(5, __LINE__);
	_scheduler.ready(_share(5));
	_update_head_and_check( 10, 430, 5, 120, __LINE__); /* 5'120 - 1°210 - 2°170 - 9'90 6 5 */

	_construct_share(4, __LINE__);
	_scheduler.yield();
	_update_head_and_check( 10, 440, 9,  90, __LINE__); /* 5'0 - 1°170 - 4°90 - 2°170 - 9'90 6 5 */

	_set_share_ready_and_check(4, true, __LINE__);
	_scheduler.yield();
	_update_head_and_check( 50, 490, 4,  90, __LINE__); /* 5'0 - 1°170 - 4'90 - 2°170 - 6'100 5 4 9 */

	_destroy_share(6, __LINE__);
	_scheduler.yield();
	_update_head_and_check( 10, 500, 5, 100, __LINE__); /* 5'0 - 1°170 - 4'0 - 2°170 - 5'100 4 9 */

	_destroy_share(9, __LINE__);
	_update_head_and_check(200, 600, 4, 100, __LINE__); /* 5'0 - 1°170 - 4'0 - 2°170 - 4'100 5 */

	_construct_share(7, __LINE__);
	_construct_share(8, __LINE__);
	_update_head_and_check(200, 700, 5, 100, __LINE__); /* 5'0 - 1°170 7°180 8°100 - 4'0 - 2°170 - 5'100 4 */

	_set_share_ready_and_check(1, true, __LINE__);
	_set_share_ready_and_check(7, true, __LINE__);
	_update_head_and_check( 10, 710, 7, 180, __LINE__); /* 5'0 - 7'180 1'170 8°100 - 4'0 - 2°170 - 5'90 4 1 7 */

	_set_share_ready_and_check(8, true, __LINE__);
	_update_head_and_check( 40, 750, 8, 100, __LINE__); /* 5'0 - 8'100 7'140 1'170 - 4'0 - 2°170 - 5'90 4 1 7 8 */

	_destroy_share(7, __LINE__);
	_update_head_and_check(200, 850, 1, 150, __LINE__); /* 5'0 - 1'170 8'0 - 4'0 - 2°170 - 5'90 4 1 8 */
	_update_head_and_check( 50, 900, 1, 100, __LINE__); /* 5'0 - 1'120 8'0 - 4'0 - 2°170 - 5'90 4 1 8 */

	_scheduler.yield();
	_update_head_and_check( 60, 960, 5,  40, __LINE__); /* 5'0 - 8'0 1'0 - 4'0 - 2°170 - 5'90 4 1 8 */
	_update_head_and_check(100,   0, 5, 120, __LINE__); /* 5'120 - 8'100 1'230 - 4'90 - 2°170 - 5'50 4 1 8 */


	/**********************************************
	 ** Round #7: Re-configure quota, first part **
	 **********************************************/

	_scheduler.quota(_share(5), 100);
	_construct_share(6, __LINE__);
	_update_head_and_check( 40,  40, 5,  80, __LINE__); /* 5'80 - 8'100 1'230 - 4'90 - 2°170 - 5'50 4 1 8 */

	_scheduler.quota(_share(5), 70);
	_scheduler.ready(_share(6));
	_update_head_and_check( 10,  50, 5,  70, __LINE__); /* 5'70 - 8'100 1'230 - 4'90 - 2°170 - 5'50 4 1 8 6 */

	_scheduler.quota(_share(5), 40);
	_construct_share(9, __LINE__);
	_update_head_and_check( 10,  60, 5,  40, __LINE__); /* 5'40 - 8'100 1'230 - 4'90 - 2°170 - 5'50 4 1 8 6 */

	_scheduler.quota(_share(5), 0);
	_scheduler.ready(_share(9));
	_update_head_and_check( 20,  80, 8, 100, __LINE__); /* 8'100 1'230 - 4'90 - 2°170 - 5'50 4 1 8 6 9 */

	_scheduler.quota(_share(8), 120);
	_update_head_and_check( 30, 110, 8,  70, __LINE__); /* 8'70 1'230 - 4'90 - 2°170 - 5'50 4 1 8 6 9 */

	_scheduler.quota(_share(9), 40);
	_update_head_and_check( 10, 120, 8,  60, __LINE__); /* 8'60 1'230 9'0 - 4'90 - 2°170 - 5'50 4 1 8 6 9 */

	_scheduler.quota(_share(8), 130);
	_update_head_and_check( 10, 130, 8,  50, __LINE__); /* 8'50 1'230 9'0 - 4'90 - 2°170 - 5'50 4 1 8 6 9 */

	_scheduler.quota(_share(8), 50);
	_update_head_and_check( 20, 150, 8,  30, __LINE__); /* 8'30 1'230 9'0 - 4'90 - 2°170 - 5'50 4 1 8 6 9 */

	_scheduler.quota(_share(6), 60);
	_update_head_and_check( 10, 160, 8,  20, __LINE__); /* 6'0 - 8'20 1'230 9'0 - 4'90 - 2°170 - 5'50 4 1 8 6 9 */

	_scheduler.unready(_share(8));
	_update_head_and_check( 10, 170, 1, 230, __LINE__); /* 6'0 - 1'230 9'0 8°10 - 4'90 - 2°170 - 5'50 4 1 6 9 */

	_scheduler.unready(_share(1));
	_update_head_and_check(100, 270, 4,  90, __LINE__); /* 6'0 - 9'0 8°10 1°130 - 4'90 - 2°170 - 5'50 4 6 9 */
	_update_head_and_check(100, 360, 5,  50, __LINE__); /* 6'0 - 9'0 8°10 1°130 - 4'0 - 2°170 - 5'50 4 6 9 */

	_scheduler.quota(_share(1), 110);
	_update_head_and_check( 10, 370, 5,  40, __LINE__); /* 6'0 - 9'0 8°10 1°110 - 4'0 - 2°170 - 5'40 4 6 9 */

	_scheduler.quota(_share(1), 120);
	_update_head_and_check( 20, 390, 5,  20, __LINE__); /* 6'0 - 9'0 8°10 1°110 - 4'0 - 2°170 - 5'20 4 6 9 */

	_scheduler.quota(_share(4), 210);
	_update_head_and_check( 10, 400, 5,  10, __LINE__); /* 6'0 - 9'0 8°10 1°110 - 4'0 - 2°170 - 5'10 4 6 9 */

	_scheduler.ready(_share(1));
	_update_head_and_check( 20, 410, 1, 110, __LINE__); /* 6'0 - 1'110 9'0 8°10 - 4'0 - 2°170 - 4'100 6 9 1 5 */

	_scheduler.quota(_share(1), 100);
	_update_head_and_check( 30, 440, 1,  80, __LINE__); /* 6'0 - 1'80 9'0 8°10 - 4'0 - 2°170 - 4'100 6 9 1 5 */

	_set_share_ready_and_check(8, true, __LINE__);
	_update_head_and_check( 10, 450, 8,  10, __LINE__); /* 6'0 - 8'10 1'70 9'0 - 4'0 - 2°170 - 4'100 6 9 1 5 8 */

	_set_share_ready_and_check(2, false, __LINE__);
	_update_head_and_check( 10, 460, 1,  70, __LINE__); /* 6'0 - 1'70 9'0 8'0 - 4'0 - 2'170 - 4'100 6 9 1 5 8 2 */

	_scheduler.quota(_share(1), 20);
	_update_head_and_check( 30, 490, 1,  20, __LINE__); /* 6'0 - 1'20 9'0 8'0 - 4'0 - 2'170 - 4'100 6 9 1 5 8 2 */

	_scheduler.quota(_share(1), 50);
	_update_head_and_check( 10, 500, 1,  10, __LINE__); /* 6'0 - 1'10 9'0 8'0 - 4'0 - 2'170 - 4'100 6 9 1 5 8 2 */

	_scheduler.quota(_share(1), 0);
	_update_head_and_check( 30, 510, 2, 170, __LINE__); /* 6'0 - 9'0 8'0 - 4'0 - 2'170 - 4'100 6 9 1 5 8 2 */

	_scheduler.quota(_share(2), 180);
	_update_head_and_check( 50, 560, 2, 120, __LINE__); /* 6'0 - 9'0 8'0 - 4'0 - 2'120 - 4'100 6 9 1 5 8 2 */

	_scheduler.unready(_share(2));
	_scheduler.quota(_share(4), 80);
	_update_head_and_check( 70, 630, 4, 100, __LINE__); /* 6'0 - 9'0 8'0 - 4'0 - 2°50 - 4'100 6 9 1 5 8 */
	_update_head_and_check( 50, 680, 4,  50, __LINE__); /* 6'0 - 9'0 8'0 - 4'0 - 2°50 - 4'100 6 9 1 5 8 */

	_scheduler.unready(_share(4));
	_update_head_and_check( 10, 690, 6, 100, __LINE__); /* 6'0 - 9'0 8'0 - 4°0 - 2°50 - 6'100 9 1 5 8 */

	_construct_share(3, __LINE__);
	_update_head_and_check( 30, 720, 6,  70, __LINE__); /* 6'0 3°110 - 9'0 8'0 - 4°0 - 2°50 - 6'70 9 1 5 8 */

	_scheduler.quota(_share(3), 210);
	_scheduler.yield();
	_update_head_and_check( 20, 740, 9, 100, __LINE__); /* 6'0 3°110 - 9'0 8'0 - 4°0 - 2°50 - 9'100 1 5 8 6 */

	_scheduler.unready(_share(9));
	_set_share_ready_and_check(4, false, __LINE__);
	_update_head_and_check( 50, 790, 1, 100, __LINE__); /* 6'0 3°110 - 9°0 8'0 - 4'0 - 2°50 - 1'100 5 8 6 4 */

	_set_share_ready_and_check(2, true, __LINE__);
	_update_head_and_check(170, 890, 2,  50, __LINE__); /* 6'0 3°110 - 9°0 8'0 - 4'0 - 2'50 - 5'100 8 6 4 2 1 */

	_scheduler.yield();
	_set_share_ready_and_check(9, false, __LINE__);
	_update_head_and_check( 60, 940, 5,  60, __LINE__); /* 6'0 3°110 - 9'0 8'0 - 4'0 - 2'0 - 5'100 8 6 4 2 1 9 */

	_scheduler.unready(_share(6));
	_update_head_and_check( 20, 960, 5,  40, __LINE__); /* 6°0 3°110 - 9'0 8'0 - 4'0 - 2'0 - 5'80 8 4 2 1 9 */

	_scheduler.yield();
	_update_head_and_check( 10, 970, 8,  30, __LINE__); /* 6°0 3°110 - 9'0 8'0 - 4'0 - 2'0 - 8'100 4 2 1 9 5 */

	_scheduler.unready(_share(8));
	_scheduler.yield();
	_update_head_and_check( 10, 980, 4,  20, __LINE__); /* 6°0 3°110 - 9'0 8°0 - 4'0 - 2'0 - 4'100 2 1 9 5 */

	_scheduler.yield();
	_update_head_and_check( 20,   0, 9,  40, __LINE__); /* 6°60 3°210 - 9'40 8°50 - 4'80 - 2'180 - 2'100 1 9 5 4 */


	/***********************************************
	 ** Round #8: Re-configure quota, second part **
	 ***********************************************/

	_scheduler.ready(_share(3));
	_update_head_and_check( 30,  30, 3, 210, __LINE__); /* 3'210 6°60 - 9'10 8°50 - 4'80 - 2'180 - 2'100 1 9 5 4 3 */

	_scheduler.unready(_share(3));
	_update_head_and_check(110, 140, 9,  10, __LINE__); /* 6°60 3°100 - 9'10 8°50 - 4'80 - 2'180 - 2'100 1 9 5 4 */
	_update_head_and_check( 40, 150, 4,  80, __LINE__); /* 6°60 3°100 - 9'0 8°50 - 4'80 - 2'180 - 2'100 1 9 5 4 */
	_update_head_and_check(100, 230, 2, 180, __LINE__); /* 6°60 3°100 - 9'0 8°50 - 4'0 - 2'180 - 2'100 1 9 5 4 */

	_scheduler.quota(_share(2), 90);
	_update_head_and_check( 40, 270, 2,  90, __LINE__); /* 6°60 3°100 - 9'0 8°50 - 4'0 - 2'90 - 2'100 1 9 5 4 */

	_scheduler.yield();
	_scheduler.quota(_share(8), 130);
	_update_head_and_check( 40, 310, 2, 100, __LINE__); /* 6°60 3°100 - 9'0 8°50 - 4'0 - 2'0 - 2'100 1 9 5 4 */
	_update_head_and_check(100, 410, 1, 100, __LINE__); /* 6°60 3°100 - 9'0 8°50 - 4'0 - 2'0 - 1'100 9 5 4 2 */

	_scheduler.quota(_share(3), 80);
	_set_share_ready_and_check(3, true, __LINE__);
	_update_head_and_check( 60, 470, 3,  80, __LINE__); /* 3'80 6°60 - 9'0 8°50 - 4'0 - 2'0 - 1'40 9 5 4 2 3*/

	_set_share_ready_and_check(8, false, __LINE__);
	_scheduler.quota(_share(8), 50);
	_update_head_and_check(100, 550, 8,  50, __LINE__); /* 3'0 6°60 - 8'50 9'0 - 4'0 - 2'0 - 1'40 9 5 4 2 3 8 */
	_update_head_and_check( 20, 570, 8,  30, __LINE__); /* 3'0 6°60 - 8'30 9'0 - 4'0 - 2'0 - 1'40 9 5 4 2 3 8 */

	_set_share_ready_and_check(6, true, __LINE__);
	_scheduler.quota(_share(6), 50);
	_update_head_and_check( 10, 580, 6,  50, __LINE__); /* 6'50 3'0 - 8'20 9'0 - 4'0 - 2'0 - 1'40 9 5 4 2 3 8 6 */
	_update_head_and_check( 70, 630, 8,  20, __LINE__); /* 6'0 3'0 - 9'0 8'20 - 4'0 - 2'0 - 1'40 9 5 4 2 3 8 6 */
	_update_head_and_check( 40, 650, 1,  40, __LINE__); /* 6'0 3'0 - 9'0 8'0 - 4'0 - 2'0 - 1'40 9 5 4 2 3 8 6 */
	_update_head_and_check( 40, 690, 9, 100, __LINE__); /* 6'0 3'0 - 9'0 8'0 - 4'0 - 2'0 - 9'100 5 4 2 3 8 6 1 */

	_destroy_share(6, __LINE__);
	_update_head_and_check(200, 790, 5, 100, __LINE__); /* 3'0 - 9'0 8'0 - 4'0 - 2'0 - 5'100 4 2 3 8 1 9 */

	_destroy_share(3, __LINE__);
	_update_head_and_check(100, 890, 4, 100, __LINE__); /* 9'0 8'0 - 4'0 - 2'0 - 4'100 2 8 1 9 5 */
	_update_head_and_check(120, 990, 2,  10, __LINE__); /* 9'0 8'0 - 4'0 - 2'0 - 2'100 8 1 9 5 4 */
	_update_head_and_check( 80,   0, 9,  40, __LINE__); /* 9'40 8'50 - 4'80 - 2'90 - 2'90 8 1 9 5 4 */


	_env.parent().exit(0);
}


void Component::construct(Env &env)
{
	static Cpu_scheduler_test::Main main { env };
}
