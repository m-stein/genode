/*
 * \brief  Basic types regarding the CBE
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LOCAL_CBE_TYPES_H_
#define _LOCAL_CBE_TYPES_H_

/* base includes */
#include <base/stdint.h>

/* gems includes */
#include <cbe/types.h>

namespace Cbe {

	enum {
		INVALID_PBA = 0xffff'ffff'ffff'ffff,
		INVALID_NODE_INDEX = 0xff,
		HASH_SIZE = 32,
		TYPE_1_NODE_STORAGE_SIZE = 64,
		TYPE_2_NODE_STORAGE_SIZE = 64,
		NR_OF_TYPE_2_NODES_PER_BLK =
			BLOCK_SIZE / TYPE_2_NODE_STORAGE_SIZE,

		NR_OF_TYPE_1_NODES_PER_BLK =
			BLOCK_SIZE / TYPE_1_NODE_STORAGE_SIZE,

		TREE_MAX_LEVEL = 6,
		TREE_MAX_NR_OF_LEVELS = TREE_MAX_LEVEL + 1,
		T2_NODE_LVL = 1,
		LOWEST_T1_NODE_LVL = 2,
		HIGHEST_T1_NODE_LVL = TREE_MAX_LEVEL,
	};


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
	}
	__attribute__((packed));

	static_assert(sizeof(Type_2_node) == TYPE_2_NODE_STORAGE_SIZE);


	struct Type_2_node_block
	{
		Type_2_node nodes[NR_OF_TYPE_2_NODES_PER_BLK] { };
	}
	__attribute__((packed));

	static_assert(sizeof(Type_2_node_block) == BLOCK_SIZE);


	struct Type_1_info
	{
		enum State {
			INVALID, READ, READ_COMPLETE, WRITE, WRITE_COMPLETE, COMPLETE };

		State             state   { INVALID };
		Type_1_node       node    { };
		Type_1_node_block entries { };
		Genode::uint8_t   index   { INVALID_NODE_INDEX };
		bool              dirty   { false };
		bool              volatil { false };
	};


	struct Type_2_info
	{
		enum State {
			INVALID, READ, READ_COMPLETE, WRITE, WRITE_COMPLETE, COMPLETE };

		State             state   { INVALID };
		Type_1_node       node    { };
		Type_2_node_block entries { };
		Genode::uint8_t   index   { INVALID_NODE_INDEX };
		bool              volatil { false };
	};
}

#endif /* _LOCAL_CBE_TYPES_H_ */
