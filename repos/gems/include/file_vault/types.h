/*
 * \brief  Common types
 * \author Martin Stein
 * \date   2021-02-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FILE_VAULT__TYPES_H_
#define _FILE_VAULT__TYPES_H_

/* base includes */
#include <util/string.h>
#include <util/xml_node.h>
#include <util/xml_generator.h>

namespace Genode { }

namespace File_vault {

	using namespace Genode;

	using Node_name = String<32>;
	using File_path = String<32>;

	class Tree_geometry
	{
		private:

			uint64_t const _nr_of_levels;
			uint64_t const _nr_of_children;
			uint64_t const _nr_of_leaves;

		public:

			Tree_geometry(
				uint64_t nr_of_levels,
				uint64_t nr_of_children,
				uint64_t nr_of_leaves)
			:
				_nr_of_levels   { nr_of_levels   },
				_nr_of_children { nr_of_children },
				_nr_of_leaves   { nr_of_leaves   }
			{ }

			uint64_t nr_of_levels()   const { return _nr_of_levels  ; }
			uint64_t nr_of_children() const { return _nr_of_children; }
			uint64_t nr_of_leaves()   const { return _nr_of_leaves  ; }
	};

	struct Operation_id { uint64_t value; };

	struct Rekey_config
	{
		Operation_id id { };

		Rekey_config(Xml_node const &node) : id(node.attribute_value("id", 0ULL)) { }

		Rekey_config(Operation_id id) : id(id) { }

		void generate(Xml_generator &xml) { xml.attribute("id", id.value); }
	};

	struct Rekey_report
	{
		Operation_id id { };
		bool finished { };

		Rekey_report(Xml_node const &node)
		: id(node.attribute_value("id", 0ULL)), finished(node.attribute_value("finished", false)) { }

		Rekey_report(Operation_id id, bool finished) : id(id), finished(finished) { }

		void generate(Xml_generator &xml)
		{
			xml.attribute("id", id.value);
			xml.attribute("finished", finished);
		}
	};
}

#endif /* _FILE_VAULT__TYPES_H_ */
