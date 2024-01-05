/*
 * \brief  Module for checking all hashes of a VBD snapshot
 * \author Martin Stein
 * \date   2023-05-03
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__VBD_CHECK_H_
#define _TRESOR__VBD_CHECK_H_

/* tresor includes */
#include <tresor/types.h>
#include <tresor/block_io.h>

namespace Tresor { class Vbd_check; }

class Tresor::Vbd_check
{
	private:

		NONCOPYABLE(Vbd_check);

	public:

		class Check
		{
			public:

				using Module = Vbd_check;

				struct Attr
				{
					Tree_root const &in_vbd;
					bool &out_success;
				};

			private:

				enum State { INIT, IN_PROGRESS, COMPLETE, READ_BLK, READ_BLK_SUCCEEDED };

				Attr _attr;
				State _state { INIT };
				Type_1_node_block_walk _t1_blks { };
				bool _check_node[TREE_MAX_NR_OF_LEVELS][NUM_NODES_PER_BLK] { };
				Block _blk { };
				Number_of_leaves _num_remaining_leaves { 0 };
				Generated_request<Check, Block_io_read, State> _read_block { *this, _state, INIT };

				/*
				 * ANMERKUNG
				 *
				 * Statt wie bislang eine feste Anzahl von Channels im
				 * jeweiligen Ziel-Modul zu halten, hat jetzt jedes
				 * Quell-Modul ein Request+Channel-Member
				 * pro benötigtem Request-Typ. Das wird definitiv
				 * mehr RAM benötigen. Beispiel:
				 * Free-Tree Request+Channel > 32K
				 *
				 * Der Tresor-Tester muß diesbezüglich umgebaut werden, da er
				 * für jedes User-Command ein Command-Objekt mit einem union
				 * über mögliche Requests (check request, tresor request,
				 * init request, etc.) hält und das über die gesamte Laufzeit.
				 * Siehe stark gestiegener RAM/CAP Verbrauch in tresor_tester.run.
				 */

				NONCOPYABLE(Check);

				void _mark_succeeded(bool &);

				bool _execute_node(Block_io &, Tree_level_index, Tree_node_index, bool &);

			public:

				Check(Attr attr) : _attr(attr) { }

				void print(Output &out) const { Genode::print(out, "check ", _attr.in_vbd); }

				void mark_failed(bool &, Error_string);

				bool execute(Block_io &);

				bool complete() const { return _state == COMPLETE; }
		};

		Vbd_check() { }

		/*
		 * ANMERKUNG
		 *
		 * Mit dieser Mechanik kann ein Modul nicht mehr, wie vorher,
		 * eigenes Policies auf die Annahme von Requests beziehungsweise
		 * die Reihenfolge der Abarbeitung anwenden. Beziehungen zwischen
		 * Requests eines Moduls können nicht mehr lokal umgesetzt werden
		 * sondern müssen vom Top-Level-Modul durch das grob-granularere
		 * Scheduling verwirklicht werden. Hier habe ich noch kein Beispiel, da
		 * wir bislang die Fähigkeit der Tresor-Lib zur Parallelität nicht
		 * ausreizen.
		 *
		 * Zudem bedingt das, daß das Top-Level Modul immer der Scheduler ist
		 * welcher dann nicht über die übliche Modul-Mechanik getrieben werden
		 * kann. Daraus resultiert Anpassungensbedarf, da "über" dem Scheduler
		 * derzeit noch das Command-Modul mit eigener Logik (VFS: Splitter,
		 * Tester: Test-Auswertung) liegt.
		 */
		bool execute(Check &req, Block_io &block_io) { return req.execute(block_io); }
};

#endif /* _TRESOR__VBD_CHECK_H_ */
