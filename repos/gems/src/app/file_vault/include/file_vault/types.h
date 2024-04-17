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
#include <tresor/types.h>

namespace Genode { }

namespace File_vault {

	using namespace Tresor;
	using namespace Genode;

	using Node_name = String<32>;
	using File_path = String<32>;

	static constexpr Tree_degree TRESOR_VBD_DEGREE = 64;
	static constexpr Tree_level_index TRESOR_VBD_MAX_LVL = 5;
	static constexpr Tree_degree TRESOR_FREE_TREE_DEGREE = 64;
	static constexpr Tree_level_index TRESOR_FREE_TREE_MAX_LVL = 5;
	static constexpr size_t MIN_PASSPHRASE_LENGTH = 8;

	enum {
		MIN_CLIENT_FS_SIZE = 100 * 1024,
		STATE_STRING_CAPACITY = 64,
		TRESOR_BLOCK_SIZE = 4096,
		MAIN_FRAME_WIDTH = 46,
		TRESOR_NR_OF_SUPERBLOCKS = 8,
	};

	struct Number_of_clients { uint64_t value; };
	struct Operation_id { uint64_t value; };

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

	using Version_string = String<80>;

	template <typename T>
	static void read_optional_attr(Xml_node const &node, char const *attr, Constructible<T> &dst)
	{
		if (node.has_attribute(attr))
			dst.construct(node.attribute_value(attr, T { }));
	}

	struct Ui_report
	{
		using State_string = String<32>;

		enum State {
			INVALID, UNINITIALIZED, INITIALIZING, LOCKED, UNLOCKING, UNLOCKED, LOCKING };

		static State_string state_to_string(State state)
		{
			switch (state) {
			case INVALID: return "invalid";
			case UNINITIALIZED: return "uninitialized";
			case INITIALIZING: return "initializing";
			case LOCKED: return "locked";
			case UNLOCKING: return "unlocking";
			case UNLOCKED: return "unlocked";
			case LOCKING: return "locking";
			}
			ASSERT_NEVER_REACHED;
		}

		static State string_to_state(State_string str)
		{
			if (str == "uninitialized") return UNINITIALIZED;
			if (str == "initializing") return INITIALIZING;
			if (str == "locked") return LOCKED;
			if (str == "unlocking") return UNLOCKING;
			if (str == "unlocked") return UNLOCKED;
			if (str == "locking") return LOCKING;
			return INVALID;
		}

		struct Rekey
		{
			Operation_id id;
			bool finished;

			Rekey(Xml_node const &node)
			: id(node.attribute_value("id", 0ULL)), finished(node.attribute_value("finished", false)) { }

			Rekey(Operation_id id, bool finished) : id(id), finished(finished) { }

			void generate(Xml_generator &xml)
			{
				xml.attribute("id", id.value);
				xml.attribute("finished", finished);
			}
		};

		struct Extend
		{
			Operation_id id { };
			bool finished { };

			Extend(Xml_node const &node)
			: id(node.attribute_value("id", 0ULL)), finished(node.attribute_value("finished", false)) { }

			Extend(Operation_id id, bool finished) : id(id), finished(finished) { }

			void generate(Xml_generator &xml)
			{
				xml.attribute("id", id.value);
				xml.attribute("finished", finished);
			}
		};

		State state { INVALID };
		Version_string version { };
		Number_of_bytes image_size { };
		Number_of_bytes capacity { };
		Number_of_clients num_clients { };
		Constructible<Rekey> rekey { };
		Constructible<Extend> extend { };

		Ui_report() { }

		Ui_report(Xml_node const &node)
		:
			state(string_to_state(node.attribute_value("state", State_string()))),
			version(node.attribute_value("version", Version_string())),
			image_size(node.attribute_value("image_size", 0ULL)),
			capacity(node.attribute_value("capacity", 0ULL)),
			num_clients(node.attribute_value("num_clients", 0ULL))
		{
			node.with_optional_sub_node("rekey", [&] (Xml_node const &n) { rekey.construct(n); });
			node.with_optional_sub_node("extend", [&] (Xml_node const &n) { extend.construct(n); });
		}

		void generate(Xml_generator &xml)
		{
			xml.attribute("state", state_to_string(state));
			xml.attribute("version", version);
			xml.attribute("image_size", image_size);
			xml.attribute("capacity", capacity);
			xml.attribute("num_clients", num_clients.value);
			if (rekey.constructed())
				xml.node("rekey", [&] { rekey->generate(xml); });
			if (extend.constructed())
				xml.node("extend", [&] { extend->generate(xml); });
		}
	};

	struct Ui_config
	{
		struct Extend
		{
			using Tree_string = String<4>;

			enum Tree { VIRTUAL_BLOCK_DEVICE, FREE_TREE };

			Operation_id id;
			Tree tree;
			Number_of_bytes num_bytes;

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

			Extend(Xml_node const &node)
			:
				id(node.attribute_value("id", 0ULL)),
				tree(string_to_tree(node.attribute_value("tree", Tree_string()))),
				num_bytes(node.attribute_value("num_bytes", 0ULL))
			{ }

			Extend(Operation_id id, Tree tree, Number_of_bytes num_bytes) : id(id), tree(tree), num_bytes(num_bytes) { }

			void generate(Xml_generator &xml)
			{
				xml.attribute("id", id.value);
				xml.attribute("tree", tree_to_string(tree));
				xml.attribute("num_bytes", num_bytes);
			}
		};

		struct Rekey
		{
			Operation_id id;

			Rekey(Xml_node const &node) : id(node.attribute_value("id", 0ULL)) { }

			Rekey(Operation_id id) : id(id) { }

			void generate(Xml_generator &xml) { xml.attribute("id", id.value); }
		};

		Version_string version { };
		Passphrase passphrase { };
		Number_of_bytes client_fs_size { };
		Number_of_bytes journaling_buf_size { };
		Constructible<Rekey> rekey { };
		Constructible<Extend> extend { };

		Ui_config(Xml_node const &node)
		:
			version(node.attribute_value("version", Version_string())),
			passphrase(node.attribute_value("passphrase", Passphrase())),
			client_fs_size(node.attribute_value("client_fs_size", 0ULL)),
			journaling_buf_size(node.attribute_value("journaling_buf_size", 0ULL))
		{
			node.with_optional_sub_node("rekey", [&] (Xml_node const &n) { rekey.construct(n); });
			node.with_optional_sub_node("extend", [&] (Xml_node const &n) { extend.construct(n); });
		}

		Ui_config() { }

		void generate(Xml_generator &xml)
		{
			xml.attribute("passphrase", passphrase);
			xml.attribute("client_fs_size", client_fs_size);
			xml.attribute("journaling_buf_size", journaling_buf_size);
			if (rekey.constructed())
				xml.node("rekey", [&] { rekey->generate(xml); });
			if (extend.constructed())
				xml.node("extend", [&] { extend->generate(xml); });
		}

		bool passphrase_long_enough() const { return passphrase.length() >= MIN_PASSPHRASE_LENGTH + 1; }
	};

	inline size_t tresor_tree_nr_of_blocks(size_t nr_of_lvls,
	                                       size_t nr_of_children,
	                                       size_t nr_of_leafs)
	{
		size_t nr_of_blks { 0 };
		size_t nr_of_last_lvl_blks { nr_of_leafs };
		for (size_t lvl_idx { 0 }; lvl_idx < nr_of_lvls; lvl_idx++) {
			nr_of_blks += nr_of_last_lvl_blks;
			if (nr_of_last_lvl_blks % nr_of_children) {
				nr_of_last_lvl_blks = nr_of_last_lvl_blks / nr_of_children + 1;
			} else {
				nr_of_last_lvl_blks = nr_of_last_lvl_blks / nr_of_children;
			}
		}
		return nr_of_blks;
	}

	inline size_t tresor_nr_of_blocks(size_t nr_of_superblocks,
	                                  size_t nr_of_vbd_lvls,
	                                  size_t nr_of_vbd_children,
	                                  size_t nr_of_vbd_leafs,
	                                  size_t nr_of_ft_lvls,
	                                  size_t nr_of_ft_children,
	                                  size_t nr_of_ft_leafs)
	{
		size_t const nr_of_vbd_blks {
			tresor_tree_nr_of_blocks(nr_of_vbd_lvls, nr_of_vbd_children, nr_of_vbd_leafs) };

		size_t const nr_of_ft_blks {
			tresor_tree_nr_of_blocks(nr_of_ft_lvls, nr_of_ft_children, nr_of_ft_leafs) };

		/* FIXME
		 *
		 * This would be the correct way to calculate the number of MT blocks
		 * but the Tresor still uses an MT the same size as the FT for simplicity
		 * reasons. As soon as the Tresor does it right we should fix also this path.
		 *
		 *	size_t const nr_of_mt_leafs {
		 *		nr_of_ft_blks - nr_of_ft_leafs };
		 *
		 *	size_t const nr_of_mt_blks {
		 *		_tree_nr_of_blocks(
		 *			nr_of_mt_lvls,
		 *			nr_of_mt_children,
		 *			nr_of_mt_leafs) };
		 */
		size_t const nr_of_mt_blks { nr_of_ft_blks };

		return nr_of_superblocks + nr_of_vbd_blks + nr_of_ft_blks + nr_of_mt_blks;
	}

	inline Number_of_blocks tresor_tree_num_leaves(size_t payload_size)
	{
		Number_of_blocks nr_of_leaves { payload_size / TRESOR_BLOCK_SIZE };
		if (payload_size % TRESOR_BLOCK_SIZE) {
			nr_of_leaves++;
		}
		return nr_of_leaves;
	}
}

#endif /* _FILE_VAULT__TYPES_H_ */
