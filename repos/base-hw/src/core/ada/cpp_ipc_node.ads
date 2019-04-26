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
with CPP_Thread;
with IPC_Node;

package CPP_IPC_Node is

   function Object_Size (Obj : IPC_Node.Object_Type) return CPP.Uint32_Type
     with Export,
          Convention    => C,
          External_Name => "_ZN3Ada11object_sizeERKNS_8Ipc_nodeE";

   procedure Initialize_Object (
      Obj  : IPC_Node.Object_Reference_Type;
      Thrd : CPP_Thread.Object_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN3Ada8Ipc_nodeC1ERN6Kernel6ThreadE";

   procedure Deinitialize_Object (Obj : IPC_Node.Object_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN3Ada8Ipc_nodeD1Ev";

   function Can_Send_Request (Obj : IPC_Node.Object_Type)
   return CPP.Bool_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZN3Ada8Ipc_node16can_send_requestEv";

   procedure For_Each_Helper (
      Obj  : IPC_Node.Object_Reference_Type;
      Func : access procedure (Thrd : CPP_Thread.Object_Reference_Type))
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN3Ada8Ipc_node15for_each_helperEPFvRN6Kernel6ThreadEE";

   procedure Send_Request (
      Obj     : IPC_Node.Object_Reference_Type;
      Callee  : IPC_Node.Object_Reference_Type;
      Help    : CPP.Bool_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN3Ada8Ipc_node12send_requestERS0_b";

   procedure Send_Reply (Obj : in out IPC_Node.Object_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN3Ada8Ipc_node10send_replyEv";

   function Can_Wait_For_Request (Obj : IPC_Node.Object_Type)
   return CPP.Bool_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZN3Ada8Ipc_node17can_await_requestEv";

   procedure Wait_For_Request (Obj : IPC_Node.Object_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN3Ada8Ipc_node13await_requestEv";

   function Helping_Sink (Obj : IPC_Node.Object_Reference_Type)
   return CPP_Thread.Object_Reference_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZN3Ada8Ipc_node12helping_sinkEv";

   procedure Cancel_Waiting (Obj : IPC_Node.Object_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN3Ada8Ipc_node14cancel_waitingEv";

   function Waits_For_Request (Obj : IPC_Node.Object_Type)
   return CPP.Bool_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZNK3Ada8Ipc_node14awaits_requestEv";

end CPP_IPC_Node;
