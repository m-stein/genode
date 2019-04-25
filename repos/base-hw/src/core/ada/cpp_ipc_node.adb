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

   procedure Initialize_Object (
      Obj  : IPC_Node.Object_Reference_Type;
      Thrd : CPP_Thread.Object_Reference_Type)
   is
   begin
      IPC_Node.Initialize_Object (Obj, Thrd);
   end Initialize_Object;

   function Can_Send_Request (Obj : IPC_Node.Object_Type)
   return CPP.Bool_Type
   is (CPP.Bool_From_Ada (IPC_Node.Can_Send_Request (Obj)));

   function Thread (Obj : IPC_Node.Object_Reference_Type)
   return CPP_Thread.Object_Reference_Type
   is (IPC_Node.Thread (Obj));

end CPP_IPC_Node;
