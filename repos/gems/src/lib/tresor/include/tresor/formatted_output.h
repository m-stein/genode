
/* os includes */
#include <util/formatted_output.h>

template <size_t LEN>
class Fixed_length
{
	private:

		String<LEN> const _str;

	public:

		template <typename... ARGS>
		Fixed_length(ARGS &&... args) : _str(args...) { }

		void print(Output &out) const
		{
			Genode::print(out, Left_aligned(LEN, _str));
		}
};

class Pba_allocation {

	private:

		Type_1_node_walk const &_t1_node_walk;
		Tree_walk_pbas const &_new_pbas;

	public:

		Pba_allocation(Type_1_node_walk const &t1_node_walk,
		               Tree_walk_pbas const &new_pbas)
		:
			_t1_node_walk { t1_node_walk },
			_new_pbas { new_pbas }
		{ }

		void print(Output &out) const
		{
			bool first { true };
			for (unsigned lvl { 0 }; lvl < TREE_MAX_NR_OF_LEVELS; lvl++) {

				if (_t1_node_walk.nodes[lvl].pba == _new_pbas.pbas[lvl])
					continue;

				Genode::print(out, first ? "" : ", ", _t1_node_walk.nodes[lvl].pba, " -> ", _new_pbas.pbas[lvl]);
				first = false;
			}
		}
};
