/*
 * \brief  Module for management of the superblocks
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/log.h>

/* cbe tester includes */
#include <superblock_control.h>

using namespace Genode;
using namespace Cbe;

enum { VERBOSE_SUPERBLOCK_CONTROL = 0 };


/********************************
 ** Superblock_control_request **
 ********************************/

void Superblock_control_request::create(void     *buf_ptr,
                                        size_t    buf_size,
                                        uint64_t  src_module_id,
                                        uint64_t  src_request_id,
                                        size_t    req_type,
                                        void     *prim_ptr,
                                        size_t    prim_size,
                                        uint64_t  client_req_offset,
                                        uint64_t  client_req_tag,
                                        uint64_t  vba)
{
error(__func__);
	Superblock_control_request req { src_module_id, src_request_id };
	req._type = (Type)req_type;

	if (prim_size > sizeof(req._prim)) {
		error(prim_size, " ", sizeof(req._prim));
		class Bad_size_1 { };
		throw Bad_size_1 { };
	}
	memcpy(&req._prim, prim_ptr, prim_size);
	req._client_req_offset = client_req_offset;
	req._client_req_tag = client_req_tag;
	req._vba = vba;

	if (sizeof(req) > buf_size) {
		class Bad_size_0 { };
		throw Bad_size_0 { };
	}
	memcpy(buf_ptr, &req, sizeof(req));
}


Superblock_control_request::
Superblock_control_request(unsigned long src_module_id,
                           unsigned long src_request_id)
:
	Module_request { src_module_id, src_request_id, SUPERBLOCK_CONTROL }
{ }


char const *Superblock_control_request::type_name()
{
	switch (_type) {
	case INVALID: return "invalid";
	case READ_VBA: return "read_vba";
	case WRITE_VBA: return "write_vba";
	case SYNC: return "sync";
	case INITIALIZE: return "initialize";
	case DEINITIALIZE: return "deinitialize";
	}
	return "?";
}


/************************
 ** Superblock_control **
 ************************/

bool Superblock_control::_peek_generated_request(uint8_t *,
                                                 size_t   )
{
	return false;
}


void Superblock_control::_drop_generated_request(Module_request &req)
{
	unsigned long const id { req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Bad_id { };
		throw Bad_id { };
	}
	switch (_channels[id]._state) {
	default:
		class Exception_1 { };
		throw Exception_1 { };
	}
}


void Superblock_control::execute(bool &)
{
	for (Channel &channel : _channels) {

		if (channel._state == Channel::INACTIVE)
			continue;

		switch (channel._request._type) {
		default:
			class Exception_1 { };
			throw Exception_1 { };
		}
	}
}


void Superblock_control::generated_request_complete(Module_request &req)
{
	unsigned long const id { req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	switch (_channels[id]._state) {
	default:
		class Exception_2 { };
		throw Exception_2 { };
	}
}


bool Superblock_control::_peek_completed_request(uint8_t *buf_ptr,
                                     size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::COMPLETE) {
			if (sizeof(channel._request) > buf_size) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			memcpy(buf_ptr, &channel._request, sizeof(channel._request));
			return true;
		}
	}
	return false;
}


void Superblock_control::_drop_completed_request(Module_request &req)
{
	unsigned long id { 0 };
	id = req.dst_request_id();
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	if (_channels[id]._state != Channel::COMPLETE) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	_channels[id]._state = Channel::INACTIVE;
}


bool Superblock_control::ready_to_submit_request()
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::INACTIVE)
			return true;
	}
	return false;
}

void Superblock_control::submit_request(Module_request &req)
{
	for (unsigned long id { 0 }; id < NR_OF_CHANNELS; id++) {
		if (_channels[id]._state == Channel::INACTIVE) {
			req.dst_request_id(id);
			_channels[id]._request = *dynamic_cast<Request *>(&req);
			_channels[id]._state = Channel::SUBMITTED;
			return;
		}
	}
	class Invalid_call { };
	throw Invalid_call { };
}



/* utilities


   --
   --  Max_VBA
   --
   function Max_VBA (Obj : Object_Type)
   return Virtual_Block_Address_Type
   is (
      Superblock_Control.Max_VBA (Obj.Superblock));

   --
   --  Info
   --
   procedure Info (
      Obj  :     Object_Type;
      Info : out Info_Type)
   is
   begin
      Superblock_Control.Info (Obj.Superblock, Info);
   end Info;


   --
   --  Active_Snapshot_IDs
   --
   procedure Active_Snapshot_IDs (
      Obj  :     Object_Type;
      List : out Active_Snapshot_IDs_Type)
   is
   begin
      Superblock_Control.Active_Snapshot_IDs (Obj.Superblock, List);
   end Active_Snapshot_IDs;



   --
   --  Idx_Of_Any_Invalid_Snap
   --
   function Idx_Of_Any_Invalid_Snap (Snapshots : Snapshots_Type)
   return Snapshots_Index_Type
   is
   begin
      Find_Invalid_Snap_Idx :
      for Idx in Snapshots'Range loop
         if not Snapshots (Idx).Valid then
            return Idx;
         end if;
      end loop Find_Invalid_Snap_Idx;
      raise Program_Error;
   end Idx_Of_Any_Invalid_Snap;


      Trans_Data        : Translation_Data_Type;
      Cur_SB            : Superblocks_Index_Type;
      Cur_Gen           : Generation_Type;
      Superblock        : Superblock_Type;

      Obj.Trans_Data       := (others => (others => 0));
      Obj.Superblock := Superblock_Invalid;
      Obj.Cur_Gen := Generation_Type'First;
      Obj.Cur_SB := Superblocks_Index_Type'First;
*/


/* execute


      Superblock_Control.Execute (
         Obj.SB_Ctrl, Obj.Superblock, Obj.Cur_SB, Obj.Cur_Gen, Progress);
*/


/* Peek generated request

      Loop_Generated_FT_Rszg_Prims :
      loop
         Declare_FT_Rszg_Prim :
         declare
            Prim : constant Primitive.Object_Type :=
               Superblock_Control.Peek_Generated_FT_Rszg_Primitive (
                  Obj.SB_Ctrl);
         begin
            exit Loop_Generated_FT_Rszg_Prims when
               not Primitive.Valid (Prim);

            raise Program_Error;

         end Declare_FT_Rszg_Prim;
      end loop Loop_Generated_FT_Rszg_Prims;


      declare
         Prim : constant Primitive.Object_Type :=
            Superblock_Control.Peek_Generated_VBD_Rkg_Primitive (
               Obj.SB_Ctrl);
         Req_Type : CXX_Object_Size_Type := CXX_Object_Size_Type'Last;
         Block_Data : Block_Data_Type;
      begin
         if Primitive.Valid (Prim) then

            case Primitive.Tag (Prim) is
            when Primitive.Tag_SB_Ctrl_VBD_Rkg_Read_VBA => Req_Type := 1;
            when Primitive.Tag_SB_Ctrl_VBD_Rkg_Write_VBA => Req_Type := 2;
            when others => raise Program_Error;
            end case;
            Block_Data_From_Snapshot (Block_Data, 0, Superblock_Control.Peek_Generated_Snapshot (
                                                        Obj.SB_Ctrl, Prim, Obj.Superblock));
            Create_VBD_Req (
               Buf_Ptr          => Buf_Ptr,
               Buf_Size         => Buf_Size,
               Src_Module_Id    => 1,
               Src_Request_Id   => CXX_UInt64_Type'Last,
               Req_Type         => Req_Type,
               Prim_Ptr         => Prim'Address,
               Prim_Size        => Prim'Size / 8,
               CBE_Req_Offset   => CXX_UInt64_Type (Request.Offset (Superblock_Control.Peek_Generated_Req (Obj.SB_Ctrl, Prim))),
               CBE_Req_Tag      => CXX_UInt64_Type (Request.Tag (Superblock_Control.Peek_Generated_Req (Obj.SB_Ctrl, Prim))),
               Last_Secured_Gen => CXX_UInt32_Type (Obj.Superblock.Last_Secured_Generation),
               FT_Root_PBA_Ptr  => Obj.Superblock.Free_Number'Address,
               FT_Root_Gen_Ptr  => Obj.Superblock.Free_Gen'Address,
               FT_Root_Hash_Ptr => Obj.Superblock.Free_Hash'Address,
               FT_Max_Lvl       => CXX_UInt64_Type (Obj.Superblock.Free_Max_Level),
               FT_Degree        => CXX_UInt64_Type (Obj.Superblock.Free_Degree),
               FT_Leaves        => CXX_UInt64_Type (Obj.Superblock.Free_Leafs),
               MT_Root_PBA_Ptr  => Obj.Superblock.Meta_Number'Address,
               MT_Root_Gen_Ptr  => Obj.Superblock.Meta_Gen'Address,
               MT_Root_Hash_Ptr => Obj.Superblock.Meta_Hash'Address,
               MT_Max_Lvl       => CXX_UInt64_Type (Obj.Superblock.Meta_Max_Level),
               MT_Degree        => CXX_UInt64_Type (Obj.Superblock.Meta_Degree),
               MT_Leaves        => CXX_UInt64_Type (Obj.Superblock.Meta_Leafs),
               VBD_Degree       => CXX_UInt64_Type (Obj.Superblock.Degree),
               VBD_Highest_VBA  => CXX_UInt64_Type (Max_VBA (Obj)),
               Rekeying         => (if Obj.Superblock.State = Rekeying then 1 else 0),
               VBA              => CXX_UInt64_Type (Primitive.Block_Number (Prim)),
               Snapshot_Ptr     => Block_Data'Address,
               Snapshots_Degree => CXX_UInt32_Type (Superblock_Control.Peek_Generated_Snapshots_Degree (
                                      Obj.SB_Ctrl, Prim, Obj.Superblock)),
               Current_Gen      => CXX_UInt64_Type (Obj.Cur_Gen),
               Key_ID           => CXX_UInt32_Type (Superblock_Control.Peek_Generated_Key_ID (
                                      Obj.SB_Ctrl, Prim))
            );
            return 1;
         end if;
      end;

      --
      --  SB Control -> Cache
      --
      declare
         Prim : constant Primitive.Object_Type :=
            Superblock_Control.Peek_Generated_Cache_Primitive (Obj.SB_Ctrl);
         Req_Type : CXX_Object_Size_Type := CXX_Object_Size_Type'Last;
      begin
         if Primitive.Valid (Prim) then
            case Primitive.Operation (Prim) is
            when Sync  => Req_Type := 3;
            when others => raise Program_error;
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
               Blk_Ptr        => System.Null_Address
            );
            return 1;
         end if;
      end;

      --
      --  SB Control -> Crypto
      --
      declare
         Prim : constant Primitive.Object_Type :=
            Superblock_Control.Peek_Generated_Crypto_Primitive (
               Obj.SB_Ctrl);
      begin

         if Primitive.Valid (Prim) then

            case Primitive.Tag (Prim) is
            when Primitive.Tag_SB_Ctrl_Crypto_Add_Key =>

               declare
                  Key_Plaintext : constant Key_Plaintext_Type :=
                     Superblock_Control.Peek_Generated_Key_Plaintext (
                        Obj.SB_Ctrl, Prim);
               begin

                  Create_Crypto_Req (
                     Buf_Ptr        => Buf_Ptr,
                     Buf_Size       => Buf_Size,
                     Src_Module_Id  => 1,
                     Src_Request_Id => CXX_UInt64_Type'Last,
                     Req_Type       => 1,
                     CBE_Req_Offset => 0,
                     CBE_Req_Tag    => 0,
                     Prim_Ptr       => Prim'Address,
                     Prim_Size      => Prim'Size / 8,
                     Key_ID         => CXX_UInt32_Type (Key_Plaintext.ID),
                     Key_Plain_Ptr  => Key_Plaintext.Value'Address,
                     PBA            => 0,
                     VBA            => 0,
                     Plain_Blk_Ptr  => System.Null_Address,
                     Cipher_Blk_Ptr => System.Null_Address
                  );
                  return 1;

               end;

            when Primitive.Tag_SB_Ctrl_Crypto_Remove_Key =>

               Create_Crypto_Req (
                  Buf_Ptr        => Buf_Ptr,
                  Buf_Size       => Buf_Size,
                  Src_Module_Id  => 1,
                  Src_Request_Id => CXX_UInt64_Type'Last,
                  Req_Type       => 2,
                  CBE_Req_Offset => 0,
                  CBE_Req_Tag    => 0,
                  Prim_Ptr       => Prim'Address,
                  Prim_Size      => Prim'Size / 8,
                  Key_ID         =>
                     CXX_UInt32_Type (
                        Superblock_Control.Peek_Generated_Key_ID (
                           Obj.SB_Ctrl, Prim)),

                  Key_Plain_Ptr  => System.Null_Address,
                  PBA            => 0,
                  VBA            => 0,
                  Plain_Blk_Ptr  => System.Null_Address,
                  Cipher_Blk_Ptr => System.Null_Address
               );
               return 1;

            when others =>

               null;

            end case;
         end if;
      end;

      --
      --  SB Control -> TA
      --
      declare
         Prim : constant Primitive.Object_Type :=
            Superblock_Control.Peek_Generated_TA_Primitive (
               Obj.SB_Ctrl);

      begin
         if Primitive.Valid (Prim) then

            case Primitive.Tag (Prim) is
            when Primitive.Tag_SB_Ctrl_TA_Create_Key =>

               Create_Trust_Anchor_Req (
                  Buf_Ptr        => Buf_Ptr,
                  Buf_Size       => Buf_Size,
                  Src_Module_Id  => 1,
                  Src_Request_Id => CXX_UInt64_Type'Last,
                  Req_Type       => 1,
                  Prim_Ptr       => Prim'Address,
                  Prim_Size      => Prim'Size / 8,
                  Key_Plain_Ptr  => System.Null_Address,
                  Key_Cipher_Ptr => System.Null_Address,
                  Passphrase_Ptr => System.Null_Address,
                  Hash_Ptr       => System.Null_Address
               );
               return 1;

            when Primitive.Tag_SB_Ctrl_TA_Encrypt_Key =>

               declare
                  Key_Plaintext : constant Key_Value_Plaintext_Type :=
                     Superblock_Control.Peek_Generated_Key_Value_Plaintext (
                        Obj.SB_Ctrl, Prim);
               begin

                  Create_Trust_Anchor_Req (
                     Buf_Ptr        => Buf_Ptr,
                     Buf_Size       => Buf_Size,
                     Src_Module_Id  => 1,
                     Src_Request_Id => CXX_UInt64_Type'Last,
                     Req_Type       => 2,
                     Prim_Ptr       => Prim'Address,
                     Prim_Size      => Prim'Size / 8,
                     Key_Plain_Ptr  => Key_Plaintext'Address,
                     Key_Cipher_Ptr => System.Null_Address,
                     Passphrase_Ptr => System.Null_Address,
                     Hash_Ptr       => System.Null_Address
                  );
                  return 1;
               end;

            when Primitive.Tag_SB_Ctrl_TA_Decrypt_Key =>

               declare
                  Key_Ciphertext : constant Key_Value_Ciphertext_Type :=
                     Superblock_Control.Peek_Generated_Key_Value_Ciphertext (
                        Obj.SB_Ctrl, Prim);
               begin

                  Create_Trust_Anchor_Req (
                     Buf_Ptr        => Buf_Ptr,
                     Buf_Size       => Buf_Size,
                     Src_Module_Id  => 1,
                     Src_Request_Id => CXX_UInt64_Type'Last,
                     Req_Type       => 3,
                     Prim_Ptr       => Prim'Address,
                     Prim_Size      => Prim'Size / 8,
                     Key_Plain_Ptr  => System.Null_Address,
                     Key_Cipher_Ptr => Key_Ciphertext'Address,
                     Passphrase_Ptr => System.Null_Address,
                     Hash_Ptr       => System.Null_Address
                  );
                  return 1;
               end;

            when Primitive.Tag_SB_Ctrl_TA_Secure_SB =>

               declare
                  Hash : constant Hash_Type :=
                     Superblock_Control.Peek_Generated_Hash (
                        Obj.SB_Ctrl, Prim);
               begin

                  Create_Trust_Anchor_Req (
                     Buf_Ptr        => Buf_Ptr,
                     Buf_Size       => Buf_Size,
                     Src_Module_Id  => 1,
                     Src_Request_Id => CXX_UInt64_Type'Last,
                     Req_Type       => 4,
                     Prim_Ptr       => Prim'Address,
                     Prim_Size      => Prim'Size / 8,
                     Key_Plain_Ptr  => System.Null_Address,
                     Key_Cipher_Ptr => System.Null_Address,
                     Passphrase_Ptr => System.Null_Address,
                     Hash_Ptr       => Hash'Address
                  );
                  return 1;
               end;

            when Primitive.Tag_SB_Ctrl_TA_Last_SB_Hash =>

               Create_Trust_Anchor_Req (
                  Buf_Ptr        => Buf_Ptr,
                  Buf_Size       => Buf_Size,
                  Src_Module_Id  => 1,
                  Src_Request_Id => CXX_UInt64_Type'Last,
                  Req_Type       => 5,
                  Prim_Ptr       => Prim'Address,
                  Prim_Size      => Prim'Size / 8,
                  Key_Plain_Ptr  => System.Null_Address,
                  Key_Cipher_Ptr => System.Null_Address,
                  Passphrase_Ptr => System.Null_Address,
                  Hash_Ptr       => System.Null_Address
               );
               return 1;

            when others =>

               null;

            end case;

         end if;

      end;

      --
      --  SB Control -> Block IO
      --
      declare
         Prim : constant Primitive.Object_Type :=
            Superblock_Control.Peek_Generated_Blk_IO_Primitive (
               Obj.SB_Ctrl);
      begin

         if Primitive.Valid (Prim) then

            case Primitive.Tag (Prim) is
            when Primitive.Tag_SB_Ctrl_Blk_IO_Write_SB =>

               Create_Block_IO_Req (
                  Buf_Ptr        => Buf_Ptr,
                  Buf_Size       => Buf_Size,
                  Src_Module_Id  => 1,
                  Src_Request_Id => CXX_UInt64_Type'Last,
                  Req_Type       => 2,
                  CBE_Req_Offset => 0,
                  CBE_Req_Tag    => 0,
                  Prim_Ptr       => Prim'Address,
                  Prim_Size      => Prim'Size / 8,
                  Key_ID         => 0,
                  PBA            =>
                     CXX_UInt64_Type (Primitive.Block_Number (Prim)),
                  VBA            => 0,
                  Blk_Count      => 1,
                  Blk_Ptr        =>
                     Superblock_Control.Peek_Generated_Blk_Data_Ptr (
                        Obj.SB_Ctrl, Prim)
               );
               return 1;

            when Primitive.Tag_SB_Ctrl_Blk_IO_Read_SB =>

               Create_Block_IO_Req (
                  Buf_Ptr        => Buf_Ptr,
                  Buf_Size       => Buf_Size,
                  Src_Module_Id  => 1,
                  Src_Request_Id => CXX_UInt64_Type'Last,
                  Req_Type       => 1,
                  CBE_Req_Offset => 0,
                  CBE_Req_Tag    => 0,
                  Prim_Ptr       => Prim'Address,
                  Prim_Size      => Prim'Size / 8,
                  Key_ID         => 0,
                  PBA            =>
                     CXX_UInt64_Type (Primitive.Block_Number (Prim)),
                  VBA            => 0,
                  Blk_Count      => 1,
                  Blk_Ptr        =>
                     Superblock_Control.Peek_Generated_Blk_Data_Ptr (
                        Obj.SB_Ctrl, Prim)
               );
               return 1;

            when Primitive.Tag_SB_Ctrl_Blk_IO_Sync =>

               Create_Block_IO_Req (
                  Buf_Ptr        => Buf_Ptr,
                  Buf_Size       => Buf_Size,
                  Src_Module_Id  => 1,
                  Src_Request_Id => CXX_UInt64_Type'Last,
                  Req_Type       => 3,
                  CBE_Req_Offset => 0,
                  CBE_Req_Tag    => 0,
                  Prim_Ptr       => Prim'Address,
                  Prim_Size      => Prim'Size / 8,
                  Key_ID         => 0,
                  PBA            =>
                     CXX_UInt64_Type (Primitive.Block_Number (Prim)),
                  VBA            => 0,
                  Blk_Count      => 1,
                  Blk_Ptr        => System.Null_Address
               );
               return 1;

            when others =>

               raise Program_Error;

            end case;
         end if;
      end;
*/


/* Drop generated_request

      when Primitive.Tag_SB_Ctrl_Crypto_Add_Key |
           Primitive.Tag_SB_Ctrl_Crypto_Remove_Key |
           Primitive.Tag_SB_Ctrl_TA_Create_Key |
           Primitive.Tag_SB_Ctrl_TA_Encrypt_Key |
           Primitive.Tag_SB_Ctrl_TA_Decrypt_Key |
           Primitive.Tag_SB_Ctrl_TA_Secure_SB |
           Primitive.Tag_SB_Ctrl_TA_Last_SB_Hash |
           Primitive.Tag_SB_Ctrl_Blk_IO_Write_SB |
           Primitive.Tag_SB_Ctrl_Blk_IO_Read_SB  |
           Primitive.Tag_SB_Ctrl_Blk_IO_Sync |
           Primitive.Tag_SB_Ctrl_VBD_Rkg_Read_VBA |
           Primitive.Tag_SB_Ctrl_VBD_Rkg_Write_VBA |
           Primitive.Tag_SB_Ctrl_Cache =>

         Superblock_Control.Drop_Generated_Primitive (Obj.SB_Ctrl, Prim);

*/

/* generated request complete


      when
         Primitive.Tag_SB_Ctrl_Crypto_Add_Key |
         Primitive.Tag_SB_Ctrl_Crypto_Remove_Key |
         Primitive.Tag_SB_Ctrl_Cache |
         Primitive.Tag_SB_Ctrl_Blk_IO_Write_SB |
         Primitive.Tag_SB_Ctrl_Blk_IO_Sync |
         Primitive.Tag_SB_Ctrl_Blk_IO_Read_SB |
         Primitive.Tag_SB_Ctrl_TA_Secure_SB |
         Primitive.Tag_SB_Ctrl_VBD_Rkg_Read_VBA =>

         Superblock_Control.Mark_Generated_Prim_Complete (
            Obj.SB_Ctrl, Prim);

      when Primitive.Tag_SB_Ctrl_VBD_Rkg_Write_VBA =>

         declare
            Snap : Snapshot_Type;
         begin
            Snapshot_From_Block_Data (Snap, Snap_Blk_Acc.all, 0);
            Superblock_Control.Mark_Generated_Prim_Complete_Snap (
               Obj.SB_Ctrl, Prim, Snap);
         end;

      when Primitive.Tag_SB_Ctrl_TA_Create_Key |
           Primitive.Tag_SB_Ctrl_TA_Decrypt_Key =>

         Superblock_Control.
            Mark_Generated_Prim_Complete_Key_Value_Plaintext (
               Obj.SB_Ctrl, Prim, Key_Plain_Acc.all);

      when Primitive.Tag_SB_Ctrl_TA_Encrypt_Key =>

         Superblock_Control.
            Mark_Generated_Prim_Complete_Key_Value_Ciphertext (
               Obj.SB_Ctrl, Prim, Key_Cipher_Acc.all);

      when Primitive.Tag_SB_Ctrl_TA_Last_SB_Hash =>

         Superblock_Control.Mark_Generated_Prim_Complete_SB_Hash (
            Obj.SB_Ctrl, Prim, Hash_Acc.all);
*/

/* peek completed request


      Loop_Completed_Prims :
      loop
         Declare_Prim :
         declare
            Prim : constant Primitive.Object_Type :=
               Superblock_Control.Peek_Completed_Primitive (Obj.SB_Ctrl);
         begin
            exit Loop_Completed_Prims when not Primitive.Valid (Prim);

            case Primitive.Tag (Prim) is
            when
               Primitive.Tag_Pool_SB_Ctrl_Read_VBA |
               Primitive.Tag_Pool_SB_Ctrl_Write_VBA |
               Primitive.Tag_Pool_SB_Ctrl_Sync |
               Primitive.Tag_Pool_SB_Ctrl_Deinitialize
            =>

               Request_Pool.Mark_Generated_Primitive_Complete (
                  Obj.Request_Pool_Obj,
                  Pool_Idx_Slot_Content (Primitive.Pool_Idx_Slot (Prim)),
                  Primitive.Success (Prim));

               Superblock_Control.Drop_Completed_Primitive (Obj.SB_Ctrl, Prim);
               Progress := True;

            when Primitive.Tag_Pool_SB_Ctrl_Initialize =>

               Request_Pool.Mark_Generated_Primitive_Complete_SB_State (
                  Obj.Request_Pool_Obj,
                  Pool_Idx_Slot_Content (Primitive.Pool_Idx_Slot (Prim)),
                  Primitive.Success (Prim),
                  Obj.Superblock.State);

               Superblock_Control.Drop_Completed_Primitive (Obj.SB_Ctrl, Prim);
               Progress := True;

            when others =>

               raise Program_Error;

            end case;

         end Declare_Prim;
      end loop Loop_Completed_Prims;
*/
