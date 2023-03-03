/*
      --
      --  Meta Tree -> Cache
      --
      declare
         Prim : constant Primitive.Object_Type :=
            Meta_Tree.Peek_Generated_Cache_Primitive (Obj.Meta_Tree_Obj);
         Req_Type : CXX_Object_Size_Type := CXX_Object_Size_Type'Last;
      begin
         if Primitive.Valid (Prim) then
            case Primitive.Operation (Prim) is
            when Read  => Req_Type := 1;
            when Write => Req_Type := 2;
            when Sync  => Req_Type := 3;
            end case;
            Create_Block_IO_Req (
               Buf_Ptr        => Buf_Ptr,
               Buf_Size       => Buf_Size,
               Src_Module_Id  => 1,
               Src_Request_Id => CXX_UInt64_Type'Last,
               Req_Type       => Req_Type,
               CBE_Req_Offset => 0,
               CBE_Req_Tag    => 0,
               Prim_Ptr       => Prim'Address,
               Prim_Size      => Prim'Size / 8,
               Key_ID         => 0,
               PBA            => CXX_UInt64_Type (Primitive.Block_Number (Prim)),
               VBA            => 0,
               Blk_Count      => 1,
               Blk_Ptr        => Meta_Tree.Peek_Generated_Cache_Data_Ptr (Obj.Meta_Tree_Obj, Prim)
            );
            return 1;
         end if;
      end;
*/

/* cbe tester includes */
#include <meta_tree.h>

using namespace Genode;
using namespace Cbe;


void Meta_tree_request::create(void     *buf_ptr,
                               size_t    buf_size,
                               uint64_t  src_module_id,
                               uint64_t  src_request_id,
                               size_t    req_type,
                               void     *prim_ptr,
                               size_t    prim_size,
                               void     *mt_root_pba_ptr,
                               void     *mt_root_gen_ptr,
                               void     *mt_root_hash_ptr,
                               uint64_t  mt_max_lvl,
                               uint64_t  mt_edges,
                               uint64_t  mt_leaves,
                               uint64_t  curr_gen,
                               uint64_t  old_pba)
{
	Meta_tree_request req { src_module_id, src_request_id };
	req._type             = (Type)req_type;
	req._mt_root_pba_ptr  = (addr_t)mt_root_pba_ptr;
	req._mt_root_gen_ptr  = (addr_t)mt_root_gen_ptr;
	req._mt_root_hash_ptr = (addr_t)mt_root_hash_ptr;
	req._mt_max_lvl       = mt_max_lvl;
	req._mt_edges         = mt_edges;
	req._mt_leaves        = mt_leaves;
	req._curr_gen         = curr_gen;
	req._old_pba          = old_pba;
	if (prim_ptr != nullptr) {
		if (prim_size > sizeof(req._prim)) {
			error(prim_size, " ", sizeof(req._prim));
			class Exception_1 { };
			throw Exception_1 { };
		}
		memcpy(&req._prim, prim_ptr, prim_size);
	}
	if (sizeof(req) > buf_size) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	memcpy(buf_ptr, &req, sizeof(req));
}


Meta_tree_request::Meta_tree_request(unsigned long src_module_id,
                             unsigned long src_request_id)
:
	Module_request { src_module_id, src_request_id, META_TREE }
{ }


char const *Meta_tree_request::type_to_string(Type type)
{
	switch (type) {
	case INVALID: return "invalid";
	case COW_UPDATE: return "cow_update";
	}
	return "?";
}
