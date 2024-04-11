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

/* Genode includes */
#include <util/string.h>
#include <util/xml_node.h>
#include <util/xml_generator.h>
#include <base/log.h>
#include <base/sleep.h>

#define ASSERT(condition) \
	do { \
		if (!(condition)) { \
			Genode::error(__FILE__, ":", __LINE__, ": ", " assertion \"", #condition, "\" failed "); \
			Genode::sleep_forever(); \
		} \
	} while (false)

#define ASSERT_NEVER_REACHED \
	do { \
		Genode::error(__FILE__, ":", __LINE__, ": ", " should have never been reached"); \
		Genode::sleep_forever(); \
	} while (false)

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

	enum { BLOCK_SIZE = 4096 };

	struct Operation_id { uint64_t value; };

	struct Rekey_config
	{
		Operation_id id;

		Rekey_config(Xml_node const &node) : id(node.attribute_value("id", 0ULL)) { }

		Rekey_config(Operation_id id) : id(id) { }

		void generate(Xml_generator &xml) { xml.attribute("id", id.value); }
	};

	struct Rekey_report
	{
		Operation_id id;
		bool finished;

		Rekey_report(Xml_node const &node)
		: id(node.attribute_value("id", 0ULL)), finished(node.attribute_value("finished", false)) { }

		Rekey_report(Operation_id id, bool finished) : id(id), finished(finished) { }

		void generate(Xml_generator &xml)
		{
			xml.attribute("id", id.value);
			xml.attribute("finished", finished);
		}
	};

	struct Extend_config
	{
		using Tree_string = String<4>;
		using Number_of_blocks = uint64_t;

		enum Tree { VIRTUAL_BLOCK_DEVICE, FREE_TREE };

		Operation_id id;
		Tree tree;
		Number_of_blocks num_blocks;

		static Tree string_to_tree(Tree_string const &str)
		{
			if (str == "vbd") return VIRTUAL_BLOCK_DEVICE;
			if (str == "ft") return FREE_TREE;
			ASSERT_NEVER_REACHED;
		}

		static Tree_string tree_to_string(Tree tree_arg)
		{
			switch (tree_arg) {
			case VIRTUAL_BLOCK_DEVICE: return "vbd";
			case FREE_TREE: return "ft"; }
			ASSERT_NEVER_REACHED;
		}

		Extend_config(Xml_node const &node)
		:
			id(node.attribute_value("id", 0ULL)),
			tree(string_to_tree(node.attribute_value("tree", Tree_string()))),
			num_blocks(node.attribute_value("id", 0ULL))
		{ }

		Extend_config(Operation_id id, Tree tree, Number_of_blocks num_blocks) : id(id), tree(tree), num_blocks(num_blocks) { }

		void generate(Xml_generator &xml)
		{
			xml.attribute("id", id.value);
			xml.attribute("tree", tree_to_string(tree));
			xml.attribute("num_blocks", num_blocks);
		}
	};

	struct Extend_report
	{
		Operation_id id { };
		bool finished { };

		Extend_report(Xml_node const &node)
		: id(node.attribute_value("id", 0ULL)), finished(node.attribute_value("finished", false)) { }

		Extend_report(Operation_id id, bool finished) : id(id), finished(finished) { }

		void generate(Xml_generator &xml)
		{
			xml.attribute("id", id.value);
			xml.attribute("finished", finished);
		}
	};
}

#endif /* _FILE_VAULT__TYPES_H_ */
