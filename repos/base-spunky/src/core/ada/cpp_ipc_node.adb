--
--  \brief  Glue code between Ada and C++ interface of IPC node
--  \author Martin stein
--  \date   2019-04-24
--

--
--  Copyright (C) 2019 Genode Labs GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

pragma Ada_2012;

package body CPP_IPC_Node is

   function Object_Size (Obj : IPC_Node.Object_Type) return CPP.Uint32_Type
   is
      use CPP;
   begin
      return Obj'Size / 8;
   end Object_Size;

   procedure Initialize_Object (
      Obj  : IPC_Node.Object_Reference_Type;
      Thrd : CPP_Thread.Object_Reference_Type)
   is
   begin
      IPC_Node.Initialize_Object (Obj, Thrd);
   end Initialize_Object;

   procedure Deinitialize_Object (Obj : IPC_Node.Object_Reference_Type)
   is
   begin
      IPC_Node.Deinitialize_Object (Obj);
   end Deinitialize_Object;

   function Can_Send_Request (Obj : IPC_Node.Object_Type)
   return CPP.Bool_Type
   is (CPP.Bool_From_Ada (IPC_Node.Can_Send_Request (Obj)));

   procedure For_Each_Helper (
      Obj  : IPC_Node.Object_Reference_Type;
      Func : access procedure (Thrd : CPP_Thread.Object_Reference_Type))
   is
   begin
      IPC_Node.For_Each_Helper (Obj, Func);
   end For_Each_Helper;

   procedure Send_Request (
      Obj     : IPC_Node.Object_Reference_Type;
      Callee  : IPC_Node.Object_Reference_Type;
      Help    : CPP.Bool_Type)
   is
   begin
      IPC_Node.Send_Request (Obj, Callee, CPP.Bool_To_Ada (Help));
   end Send_Request;

   procedure Send_Reply (Obj : in out IPC_Node.Object_Type)
   is
   begin
      IPC_Node.Send_Reply (Obj);
   end Send_Reply;

   function Can_Wait_For_Request (Obj : IPC_Node.Object_Type)
   return CPP.Bool_Type
   is (CPP.Bool_From_Ada (IPC_Node.Can_Wait_For_Request (Obj)));

   procedure Wait_For_Request (Obj : IPC_Node.Object_Reference_Type)
   is
   begin
      IPC_Node.Wait_For_Request (Obj);
   end Wait_For_Request;

   function Helping_Sink (Obj : IPC_Node.Object_Reference_Type)
   return CPP_Thread.Object_Reference_Type
   is (IPC_Node.Helping_Sink (Obj));

   procedure Cancel_Waiting (Obj : IPC_Node.Object_Reference_Type)
   is
   begin
      IPC_Node.Cancel_Waiting (Obj);
   end Cancel_Waiting;

   function Waits_For_Request (Obj : IPC_Node.Object_Type)
   return CPP.Bool_Type
   is (CPP.Bool_From_Ada (IPC_Node.Waits_For_Request (Obj)));

end CPP_IPC_Node;
