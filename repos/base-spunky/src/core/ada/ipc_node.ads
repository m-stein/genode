--
--  \brief  Peer of message-based synchronous inter-process communication
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

with CPP_Thread;
with Generic_Queue;

package IPC_Node is

   type Object_Type           is private;
   type Object_Reference_Type is not null access all Object_Type;

   procedure Initialize_Object (
      Obj  : Object_Reference_Type;
      Thrd : CPP_Thread.Object_Reference_Type);

   procedure Deinitialize_Object (Obj : Object_Reference_Type);

   function Can_Send_Request (Obj : Object_Type)
   return Boolean;

   procedure Send_Request (
      Obj     : Object_Reference_Type;
      Callee  : Object_Reference_Type;
      Help    : Boolean);

   procedure Send_Reply (Obj : in out Object_Type);

   function Can_Wait_For_Request (Obj : Object_Type)
   return Boolean;

   procedure Wait_For_Request (Obj : Object_Reference_Type);

   function Helping_Sink (Obj : Object_Reference_Type)
   return CPP_Thread.Object_Reference_Type;

   procedure For_Each_Helper (
      Obj  : Object_Reference_Type;
      Func : access procedure (Thrd : CPP_Thread.Object_Reference_Type));

   procedure Cancel_Waiting (Obj : Object_Reference_Type);

   function Waits_For_Request (Obj : Object_Type)
   return Boolean;

private

   package Queue is new Generic_Queue (
      Object_Type,
      Object_Reference_Type);

   type Object_Pointer_Type is access all Object_Type;
   type State_Type          is (Inactive, Wait_For_Reply, Wait_For_Request);

   type Object_Type is record
      Thread             : CPP_Thread.Object_Reference_Type;
      State              : State_Type;
      Caller             : Object_Pointer_Type;
      Callee             : Object_Pointer_Type;
      Help               : Boolean;
      Request_Queue      : Queue.Queue_Type;
      Request_Queue_Item : aliased Queue.Item_Type;
   end record;

   procedure Announce_Request (
      Obj  : Object_Reference_Type;
      Node : Object_Reference_Type);

   procedure Receive_Request (
      Obj    : Object_Reference_Type;
      Caller : Object_Reference_Type);

   procedure Cancel_Outgoing_Request (Obj : Object_Reference_Type);

   procedure Cancel_Incoming_Request (Obj : Object_Reference_Type);

   procedure Cancel_Request_Queue (Obj : Object_Reference_Type);

   procedure Outgoing_Request_Cancelled (Obj : Object_Reference_Type);

end IPC_Node;
