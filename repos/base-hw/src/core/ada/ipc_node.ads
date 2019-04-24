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

package IPC_Node is

   type State_Type            is (Inactive, Wait_For_Reply, Wait_For_Request);
   type Object_Type           is private;
   type Object_Pointer_Type   is          access all Object_Type;
   type Object_Reference_Type is not null access Object_Type;

   package Queue is

      type Qbject_Type           is private;
      type Ibject_Type           is private;
      type Ibject_Pointer_Type   is          access all Ibject_Type;
      type Ibject_Reference_Type is not null access all Ibject_Type;

      function Initialized_Object return Qbject_Type;

      function Initialized_Item_Object (Payload : Object_Reference_Type)
      return Ibject_Type;

      procedure Enqueue (
         Obj : in out Qbject_Type;
         Itm :        Ibject_Reference_Type);

   private

      type Qbject_Type is record
         Head : Ibject_Pointer_Type;
         Tail : Ibject_Pointer_Type;
      end record;

      type Ibject_Type is record
         Next    : Ibject_Pointer_Type;
         Payload : Object_Reference_Type;
      end record;

      function Empty (Obj : Qbject_Type)
      return Boolean;

   end Queue;

   procedure Initialize_Object (
      Obj  : Object_Reference_Type;
      Thrd : CPP_Thread.Object_Reference_Type);

   function Can_Send_Request (Obj : Object_Reference_Type)
   return Boolean;

   procedure Send_Request (
      Obj     : Object_Reference_Type;
      Callee  : Object_Reference_Type;
      Help    : Boolean);

   function Thread (Obj : Object_Reference_Type)
   return CPP_Thread.Object_Reference_Type;

private

   type Object_Type is record
      Thread             : CPP_Thread.Object_Reference_Type;
      State              : State_Type;
      Caller             : Object_Pointer_Type;
      Callee             : Object_Pointer_Type;
      Help               : Boolean;
      Request_Queue      : Queue.Qbject_Type;
      Request_Queue_Item : aliased Queue.Ibject_Type;
   end record;

   procedure Announce_Request (
      Obj  : Object_Reference_Type;
      Node : Object_Reference_Type);

   procedure Receive_Request (
      Obj    : Object_Reference_Type;
      Caller : Object_Reference_Type);

end IPC_Node;
