/*
 * \brief  Integration of the Consistent Block Encrypter (CBE)
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2020-11-10
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CBE__TYPES_H_
#define _CBE__TYPES_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/output.h>
#include <base/exception.h>
#include <util/string.h>

namespace Cbe {

	enum { INVALID_GENERATION = 0 };

	using namespace Genode;
	using Number_of_primitives   = size_t;
	using Physical_block_address = uint64_t;
	using Virtual_block_address  = uint64_t;
	using Generation             = uint64_t;
	using Generation_string      = String<21>;
	using Height                 = uint32_t;
	using Number_of_leaves       = uint64_t;
	using Number_of_leafs        = uint64_t;
	using Number_of_blocks_old   = uint64_t;
	using Degree                 = uint32_t;

	static constexpr uint32_t BLOCK_SIZE = 4096;
	static constexpr uint32_t NR_OF_SNAPSHOTS = 48;

	struct Block_data
	{
		char values[BLOCK_SIZE];

		void print(Genode::Output &out) const
		{
			using namespace Genode;
			for (unsigned idx { 0 }; idx < 16; idx++)
				Genode::print(
					out, idx && !(idx % 4) ? " " : "",
					Hex(values[idx], Hex::OMIT_PREFIX, Hex::PAD));
		}
	}
	__attribute__((packed));

	/*
	 * The Hash contains the hash of a node.
	 */
	struct Hash_old
	{
		enum { MAX_LENGTH = 32, };
		char values[MAX_LENGTH];

		/* hash as hex value plus "0x" prefix and terminating null */
		using String = Genode::String<sizeof(values) * 2 + 3>;

		/* debug */
		void print(Genode::Output &out) const
		{
			using namespace Genode;
			Genode::print(out, "0x");
			bool leading_zero = true;
			for (char const c : values) {
				if (leading_zero) {
					if (c) {
						leading_zero = false;
						Genode::print(out, Hex(c, Hex::OMIT_PREFIX));
					}
				} else {
					Genode::print(out, Hex(c, Hex::OMIT_PREFIX, Hex::PAD));
				}
			}
			if (leading_zero) {
				Genode::print(out, "0");
			}
		}
	};

	struct Key_plaintext_value
	{
		enum { KEY_SIZE = 32 };
		char value[KEY_SIZE];
	};

	struct Key_ciphertext_value
	{
		enum { KEY_SIZE = 32 };
		char value[KEY_SIZE];
	};

	/*
	 * The Key contains the key-material that is used to
	 * process cipher-blocks.
	 *
	 * (For now it is not used but the ID field is already referenced
	 *  by type 2 nodes.)
	 */
	struct Key_old
	{
		enum { KEY_SIZE = 32 };
		char value[KEY_SIZE];

		struct Id { uint32_t value; };
		Id id;

		using String = Genode::String<sizeof(value) * 2 + 3>;

		void print(Genode::Output &out) const
		{
			using namespace Genode;
			Genode::print(out, "[", id.value, ", ");
			for (uint32_t i = 0; i < 4; i++) {
				Genode::print(out, Hex(value[i], Hex::OMIT_PREFIX, Hex::PAD));
			}
			Genode::print(out, "...]");
		}
	} __attribute__((packed));


	struct Info
	{
		bool valid         { false };
		bool rekeying      { false };
		bool extending_vbd { false };
		bool extending_ft  { false };
	}
	__attribute__((packed));


	using Number_of_blocks_new   = Genode::uint64_t;
	using Tree_level_index       = Genode::uint32_t;
	using Tree_degree            = Genode::uint32_t;
	using Tree_degree_log_2      = Genode::uint32_t;
	using Tree_number_of_leaves  = Genode::uint64_t;
	using Key_id                 = Genode::uint32_t;
	using Snapshot_id            = Genode::uint32_t;
	using Snapshots_index        = Genode::uint32_t;
	using Node_index             = Genode::uint8_t;
	using Superblocks_index      = Genode::uint8_t;

	enum {
		PRIM_BUF_SIZE = 128,
		INVALID_PBA = 0xffff'ffff'ffff'ffff,
		INVALID_VBA = 0xffff'ffff'ffff'ffff,
		INVALID_NODE_INDEX = 0xff,
		HASH_SIZE = 32,
		TYPE_1_NODE_STORAGE_SIZE = 64,
		TYPE_2_NODE_STORAGE_SIZE = 64,
		NR_OF_TYPE_2_NODES_PER_BLK =
			BLOCK_SIZE / TYPE_2_NODE_STORAGE_SIZE,

		NR_OF_TYPE_1_NODES_PER_BLK =
			BLOCK_SIZE / TYPE_1_NODE_STORAGE_SIZE,

		TREE_MAX_DEGREE_LOG_2 = 6,
		TREE_MAX_DEGREE = 1 << TREE_MAX_DEGREE_LOG_2,
		TREE_MAX_LEVEL = 6,
		TREE_MAX_NR_OF_LEVELS = TREE_MAX_LEVEL + 1,
		T2_NODE_LVL = 1,
		LOWEST_T1_NODE_LVL = 2,
		HIGHEST_T1_NODE_LVL = TREE_MAX_LEVEL,
		KEY_SIZE = 32,
		MAX_NR_OF_SNAPSHOTS_PER_SB = 48,
		SNAPSHOT_STORAGE_SIZE = 72,
		NR_OF_SUPERBLOCK_SLOTS = 8,
		MAX_SUPERBLOCK_INDEX = NR_OF_SUPERBLOCK_SLOTS - 1,
		FREE_TREE_MIN_MAX_LEVEL = 2,
	};

	struct Key_value
	{
		Genode::uint8_t bytes[KEY_SIZE];
	}
	__attribute__((packed));

	struct Key_new
	{
		Key_value value;
		Key_id    id;
	}
	__attribute__((packed));


	struct Hash_new
	{
		Genode::uint8_t bytes[HASH_SIZE] { };

		void print(Genode::Output &out) const
		{
			using namespace Genode;
			for (unsigned idx { 0 }; idx < 8; idx++)
				Genode::print(
					out, idx && !(idx % 4) ? " " : "",
					Hex(bytes[idx], Hex::OMIT_PREFIX, Hex::PAD));
		}
	}
	__attribute__((packed));

	struct Type_1_node_unpadded
	{
		Genode::uint64_t pba             { 0 };
		Genode::uint64_t gen             { 0 };
		Genode::uint8_t  hash[HASH_SIZE] { 0 };
	}
	__attribute__((packed));

	struct Type_1_node
	{
		Genode::uint64_t pba             { 0 };
		Genode::uint64_t gen             { 0 };
		Genode::uint8_t  hash[HASH_SIZE] { 0 };
		Genode::uint8_t  padding[16]     { 0 };

		bool valid() const
		{
			Type_1_node node { };
			return Genode::memcmp(this, &node, sizeof(node)) != 0;
		}

		void print(Genode::Output &out) const
		{
			using namespace Genode;

			Genode::print(out, "pba: ", pba, " gen: ", gen, " hash: ");
			for (uint8_t const byte : hash)
				Genode::print(out, Hex(byte, Hex::OMIT_PREFIX, Hex::PAD));
		}
	}
	__attribute__((packed));

	static_assert(sizeof(Type_1_node) == TYPE_1_NODE_STORAGE_SIZE);


	struct Type_1_node_block
	{
		Type_1_node nodes[NR_OF_TYPE_1_NODES_PER_BLK] { };
	}
	__attribute__((packed));

	static_assert(sizeof(Type_1_node_block) == BLOCK_SIZE);


	struct Type_2_node
	{
		Genode::uint64_t pba         { 0 };
		Genode::uint64_t last_vba    { 0 };
		Genode::uint64_t alloc_gen   { 0 };
		Genode::uint64_t free_gen    { 0 };
		Genode::uint32_t last_key_id { 0 };
		Genode::uint8_t  reserved    { 0 };
		Genode::uint8_t  padding[27] { 0 };

		bool valid() const
		{
			Type_2_node node { };
			return Genode::memcmp(this, &node, sizeof(node)) != 0;
		}

		void print(Genode::Output &out) const
		{
			using namespace Genode;

			Genode::print(out, "pba: ",         pba, " "
			                   "last_vba: ",    last_vba, " "
			                   "alloc_gen: ",   alloc_gen, " "
			                   "free_gen: ",    free_gen, " "
			                   "last_key_id: ", last_key_id);
		}

	}
	__attribute__((packed));

	static_assert(sizeof(Type_2_node) == TYPE_2_NODE_STORAGE_SIZE);


	struct Type_2_node_block
	{
		Type_2_node nodes[NR_OF_TYPE_2_NODES_PER_BLK] { };
	}
	__attribute__((packed));

	static_assert(sizeof(Type_2_node_block) == BLOCK_SIZE);


	struct Block
	{
		Genode::uint8_t bytes[BLOCK_SIZE] { };
	}
	__attribute__((packed));


	struct Snapshot
	{
		Hash_new               hash;
		Physical_block_address pba;
		Generation             gen;
		Tree_number_of_leaves  nr_of_leaves;
		Tree_level_index       max_level;
		bool                   valid;
		Snapshot_id            id;
		bool                   keep;

		Genode::uint8_t        unused[6] { };

		void print(Genode::Output &out) const
		{
			using namespace Genode;

			Genode::print(out, "hash: ");
			for (uint8_t const byte : hash.bytes)
				Genode::print(out, Hex(byte, Hex::OMIT_PREFIX, Hex::PAD));

			Genode::print(out, " "
			                   "pba: ", pba, " "
			                   "gen: ", gen, " "
			                   "nr_of_leaves: ", nr_of_leaves, " "
			                   "max_level: ", max_level, " "
			                   "valid: ", valid, " "
			                   "id: ", id, " "
			                   "keep: ", keep);
		}
	}
	__attribute__((packed));

	static_assert(sizeof(Snapshot) == SNAPSHOT_STORAGE_SIZE);


	struct Snapshots
	{
		Snapshot items[MAX_NR_OF_SNAPSHOTS_PER_SB];
	}
	__attribute__((packed));


	enum Superblock_state : uint8_t
	{
		INVALID       = 0,
		NORMAL        = 1,
		REKEYING      = 2,
		EXTENDING_VBD = 3,
		EXTENDING_FT  = 4,
	};

	struct Superblock
	{
		Superblock_state       state;
		Virtual_block_address  rekeying_vba;
		Number_of_blocks_new   resizing_nr_of_pbas;
		Tree_number_of_leaves  resizing_nr_of_leaves;
		Key_new                previous_key;
		Key_new                current_key;
		Snapshots              snapshots;
		Generation             last_secured_generation;
		Snapshots_index        curr_snap;
		Tree_degree            degree;
		Physical_block_address first_pba;
		Number_of_blocks_new   nr_of_pbas;
		Generation             free_gen;
		Physical_block_address free_number;
		Hash_new               free_hash;
		Tree_level_index       free_max_level;
		Tree_degree            free_degree;
		Tree_number_of_leaves  free_leaves;
		Generation             meta_gen;
		Physical_block_address meta_number;
		Hash_new               meta_hash;
		Tree_level_index       meta_max_level;
		Tree_degree            meta_degree;
		Tree_number_of_leaves  meta_leaves;

		void initialize_invalid()
		{
			memset(this, 0, sizeof(*this));

			state                   = Superblock_state::INVALID;
			rekeying_vba            = 0;
			resizing_nr_of_pbas     = 0;
			resizing_nr_of_leaves   = 0;
			// previous_key;
			// current_key;
			// snapshots;
			last_secured_generation = 0;
			curr_snap               = 0;
			degree                  = 1;
			first_pba               = 0;
			nr_of_pbas              = 0;
			free_gen                = 0;
			free_number             = 0;
			// free_hash;
			free_max_level          = 0;
			free_degree             = 1;
			free_leaves             = 0;
			meta_gen                = 0;
			meta_number             = 0;
			// meta_hash;
			meta_max_level          = 0;
			meta_degree             = 1;
			meta_leaves             = 0;
		}

		bool valid() const { return state != INVALID; }

		void print(Genode::Output &out) const
		{
			using namespace Genode;

			Genode::print(out, "state:", (unsigned)state, " "
			                   "last_secured_generation: ", last_secured_generation, " ",
			                   "curr_snap: ", curr_snap, " ",
			                   "degree: ", degree, " ",
			                   "first_pba: ", first_pba, " "
			                   "nr_of_pbas: ", nr_of_pbas, " ");

			Genode::print(out, "snapshots:\n");
			for (auto const snap : snapshots.items)
				if (snap.valid)
					Genode::print(out, snap, "\n");
		}
	}
	__attribute__((packed));

	struct Type_1_node_walk
	{
		Type_1_node_unpadded nodes[TREE_MAX_NR_OF_LEVELS];
	}
	__attribute__((packed));

	struct Tree_walk_pbas
	{
		Physical_block_address pbas[TREE_MAX_NR_OF_LEVELS];
	}
	__attribute__((packed));

	inline Snapshots_index newest_snapshot_idx(Snapshots const &snapshots)
	{
		Snapshots_index newest_snap_idx       = 0;
		bool            newest_snap_idx_valid = false;

		for (unsigned snap_idx = 0; snap_idx < MAX_NR_OF_SNAPSHOTS_PER_SB; snap_idx ++)
		{
			auto &snapshot = snapshots.items[snap_idx];

			if (snapshot.valid and (not newest_snap_idx_valid or
			                        snapshot.gen > snapshots.items[newest_snap_idx].gen))
			{
				newest_snap_idx       = snap_idx;
				newest_snap_idx_valid = true;
			}
		}

		if (newest_snap_idx_valid)
			return newest_snap_idx;

		class Newest_snapshot_idx_error { };
		throw Newest_snapshot_idx_error { };
	}

	inline Snapshots_index idx_of_invalid_or_lowest_gen_evictable_snap(Snapshots const &snapshots,
	                                                                   Generation const curr_gen,
	                                                                   Generation const last_secured_gen)
	{
		Snapshots_index evictable_snap_idx   = 0;
		bool            evictable_snap_found = false;

		for (unsigned idx = 0; idx < MAX_NR_OF_SNAPSHOTS_PER_SB; idx ++)
		{
			auto &snapshot = snapshots.items[idx];

			if (not snapshot.valid)
				return idx;
			else
			if (not snapshot.keep and
			    snapshot.gen != curr_gen and
			    snapshot.gen != last_secured_gen)
			{
				if (not evictable_snap_found) {
					evictable_snap_found = true;
					evictable_snap_idx   = idx;
				} else
				if (snapshot.gen < snapshots.items[evictable_snap_idx].gen)
					evictable_snap_idx = idx;
			}
		}

		if (evictable_snap_found)
			return evictable_snap_idx;

		class Idx_of_invalid_or_lowest_gen_evictable_snap_error { };
		throw Idx_of_invalid_or_lowest_gen_evictable_snap_error { };
	}

	struct Active_snapshot_ids
	{
		Generation values[MAX_NR_OF_SNAPSHOTS_PER_SB] { 0 };
	}
	__attribute__((packed));
}

#endif /* _CBE__TYPES_H_ */
