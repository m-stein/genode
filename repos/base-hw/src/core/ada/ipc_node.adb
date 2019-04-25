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

package body IPC_Node is

   procedure Initialize_Object (
      Obj  : Object_Reference_Type;
      Thrd : CPP_Thread.Object_Reference_Type)
   is
   begin
      Obj.all := (
         Thread             => Thrd,
         State              => Inactive,
         Caller             => null,
         Callee             => null,
         Help               => False,
         Request_Queue      => Queue.Initialized_Object,
         Request_Queue_Item => Queue.Initialized_Item_Object (Obj));

   end Initialize_Object;

   function Can_Send_Request (Obj : Object_Type)
   return Boolean
   is (Obj.State = Inactive);

   function Can_Wait_For_Request (Obj : Object_Type)
   return Boolean
   is (Obj.State = Inactive);

   procedure Wait_For_Request (Obj : Object_Reference_Type)
   is
      use Queue;
      Req_Queue_Head : constant Queue.Ibject_Pointer_Type :=
         Queue.Head (Obj.Request_Queue);
   begin
      Obj.State := Wait_For_Request;
      if Req_Queue_Head /= null then
         Receive_Request (Obj, Queue.Item_Payload (
            Ibject_Reference_Type (Req_Queue_Head)));
         Queue.Dequeue (Obj.Request_Queue);
      end if;
   end Wait_For_Request;

   procedure Send_Request (
      Obj     : Object_Reference_Type;
      Callee  : Object_Reference_Type;
      Help    : Boolean)
   is
   begin
      Obj.State  := Wait_For_Reply;
      Obj.Callee := Object_Pointer_Type (Callee);
      Obj.Help   := False;
      Announce_Request (Callee, Obj);
      Obj.Help   := Help;
   end Send_Request;

   procedure Send_Reply (Obj : in out Object_Type)
   is
   begin
      if
         Obj.State = Inactive and
         Obj.Caller /= null
      then
         --
         --  FIXME: I simplified the second argument - could be a bug
         --
         CPP_Thread.IPC_Copy_Message (Obj.Caller.Thread, Obj.Thread);
         Obj.Caller.State := Inactive;
         CPP_Thread.IPC_Send_Request_Succeeded (Obj.Caller.Thread);
         Obj.Caller := null;
      end if;
   end Send_Reply;

   function Helping_Sink (Obj : Object_Reference_Type)
   return Object_Reference_Type
   is (
      if
         Obj.State = Wait_For_Reply and
         Obj.Help
      then
         Helping_Sink (Object_Reference_Type (Obj.Callee))
      else
         Obj);

   function Thread (Obj : Object_Reference_Type)
   return CPP_Thread.Object_Reference_Type
   is (Obj.Thread);

   package body Queue is

      function Initialized_Item_Object (Payload : Object_Reference_Type)
      return Ibject_Type
      is (
         Next    => null,
         Payload => Payload);

      function Initialized_Object return Qbject_Type
      is (
         Head => null,
         Tail => null);

      procedure Enqueue (
         Obj : in out Qbject_Type;
         Itm :        Ibject_Reference_Type)
      is
         Itm_Ptr : constant Ibject_Pointer_Type := Ibject_Pointer_Type (Itm);
      begin
         Itm.Next := null;
         if Empty (Obj) then
            Obj.Tail := Itm_Ptr;
            Obj.Head := Itm_Ptr;
         else
            Obj.Tail.Next := Itm_Ptr;
            Obj.Tail      := Itm_Ptr;
         end if;
      end Enqueue;

      procedure Dequeue (Obj : in out Qbject_Type)
      is
         Result : constant Ibject_Pointer_Type := Obj.Head;
      begin
         if Obj.Head = Obj.Tail then
            Obj.Head := null;
            Obj.Tail := null;
         else
            Obj.Head := Obj.Head.Next;
         end if;

         if Result /= null then
            Result.Next := null;
         end if;
      end Dequeue;

      function Head (Obj : Qbject_Type)
      return Ibject_Pointer_Type
      is (Obj.Head);

      function Item_Payload (Itm : Ibject_Reference_Type)
      return Object_Reference_Type
      is (Itm.Payload);

      function Empty (Obj : Qbject_Type)
      return Boolean
      is (Obj.Tail = null);

   end Queue;

   procedure Announce_Request (
      Obj  : Object_Reference_Type;
      Node : Object_Reference_Type)
   is
   begin
      if Obj.State = Wait_For_Request then
         IPC_Node.Receive_Request (Obj, Node);
         CPP_Thread.IPC_Wait_For_Request_Succeeded (Obj.Thread);
         null;
      else
         Queue.Enqueue (Obj.Request_Queue, Node.Request_Queue_Item'Access);
      end if;
   end Announce_Request;

   procedure Receive_Request (
      Obj    : Object_Reference_Type;
      Caller : Object_Reference_Type)
   is
   begin
      CPP_Thread.IPC_Copy_Message (Obj.Thread, Caller.Thread);
      Obj.Caller := Object_Pointer_Type (Caller);
      Obj.State  := Inactive;
   end Receive_Request;

end IPC_Node;
