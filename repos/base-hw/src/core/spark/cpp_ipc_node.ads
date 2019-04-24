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

with CPP;
with IPC_Node;

package CPP_IPC_Node is

   procedure Initialize_Object (
      Obj  : IPC_Node.Object_Reference_Type;
      Thrd : IPC_Node.Thread_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN14Spark_ipc_nodeC1ERN6Kernel6ThreadE";

   function Can_Send_Request (Obj : IPC_Node.Object_Reference_Type)
   return CPP.Bool_Type
   with
      Export,
      Convention    => C,
      External_Name => "X1";

   function Thread (Obj : IPC_Node.Object_Reference_Type)
   return IPC_Node.Thread_Reference_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZN14Spark_ipc_node6threadEv";

end CPP_IPC_Node;
