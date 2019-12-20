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

   procedure Deinitialize_Object (Obj : Object_Reference_Type)
   is
   begin
      Cancel_Request_Queue (Obj);
      Cancel_Incoming_Request (Obj);
      Cancel_Outgoing_Request (Obj);
   end Deinitialize_Object;

   function Can_Send_Request (Obj : Object_Type)
   return Boolean
   is (Obj.State = Inactive);

   function Waits_For_Request (Obj : Object_Type)
   return Boolean
   is (Obj.State = Wait_For_Request);

   function Can_Wait_For_Request (Obj : Object_Type)
   return Boolean
   is (Obj.State = Inactive);

   procedure Wait_For_Request (Obj : Object_Reference_Type)
   is
      use Queue;
      Req_Queue_Head : constant Queue.Item_Pointer_Type :=
         Queue.Head (Obj.Request_Queue);
   begin
      Obj.State := Wait_For_Request;
      if Req_Queue_Head /= null then
         Receive_Request (Obj, Queue.Item_Payload (
            Item_Reference_Type (Req_Queue_Head)));
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
         CPP_Thread.IPC_Copy_Message (
            Obj.Caller.Thread,
            Obj.Caller.Callee.Thread);

         Obj.Caller.State := Inactive;
         CPP_Thread.IPC_Send_Request_Succeeded (Obj.Caller.Thread);
         Obj.Caller := null;
      end if;
   end Send_Reply;

   function Helping_Sink (Obj : Object_Reference_Type)
   return CPP_Thread.Object_Reference_Type
   is (
      if
         Obj.State = Wait_For_Reply and
         Obj.Help
      then
         Helping_Sink (Object_Reference_Type (Obj.Callee))
      else
         Obj.Thread);

   procedure For_Each_Helper (
      Obj  : Object_Reference_Type;
      Func : access procedure (Thrd : CPP_Thread.Object_Reference_Type))
   is
      use Queue;
      Req_Queue_Item : Queue.Item_Pointer_Type :=
         Queue.Head (Obj.Request_Queue);
   begin
      if Obj.Caller /= null then
         if Obj.Caller.Help then
            Func (Obj.Caller.Thread);
         end if;
      end if;

      Req_Queue_Loop :
      loop
         exit Req_Queue_Loop when Req_Queue_Item = null;
         if Queue.Item_Payload (
               Queue.Item_Reference_Type (Req_Queue_Item)).Help
         then
            Func (Queue.Item_Payload (
                     Queue.Item_Reference_Type (Req_Queue_Item)).Thread);
         end if;
         Req_Queue_Item := Queue.Item_Next (
                              Queue.Item_Reference_Type (Req_Queue_Item));
      end loop Req_Queue_Loop;
   end For_Each_Helper;

   procedure Cancel_Waiting (Obj : Object_Reference_Type)
   is
   begin
      case Obj.State is
      when Wait_For_Reply =>
         Cancel_Outgoing_Request (Obj);
         Obj.State := Inactive;
         CPP_Thread.IPC_Send_Request_Failed (Obj.Thread);
      when Wait_For_Request =>
         Obj.State := Inactive;
         CPP_Thread.IPC_Wait_For_Request_Failed (Obj.Thread);
      when others =>
         null;
      end case;
   end Cancel_Waiting;

   --
   --  Outgoing_Request_Cancelled
   --
   procedure Outgoing_Request_Cancelled (Obj : Object_Reference_Type)
   is
   begin
      --  abort if there is no outgoing request
      if Obj.Callee = null then
         return;
      end if;

      --  disconnect from server and mark request as failed
      Obj.Callee := null;
      Obj.State := Inactive;
      CPP_Thread.IPC_Send_Request_Failed (Obj.Thread);

   end Outgoing_Request_Cancelled;

   --
   --  Cancel_Incoming_Request
   --
   procedure Cancel_Incoming_Request (Obj : Object_Reference_Type)
   is
   begin
      --  abort if there is no incoming request
      if Obj.Caller = null then
         return;
      end if;

      --  let the client cancel its request and disconnect from it
      Outgoing_Request_Cancelled (Object_Reference_Type (Obj.Caller));
      Obj.Caller := null;
   end Cancel_Incoming_Request;

   --
   --  Cancel_Request_Queue
   --
   procedure Cancel_Request_Queue (Obj : Object_Reference_Type)
   is
      use Queue;
      Req_Queue_Head : Queue.Item_Pointer_Type :=
         Queue.Head (Obj.Request_Queue);
   begin

      --  dequeue items from the request queue until it is empty
      Req_Queue_Loop :
      while Req_Queue_Head /= null loop

         --  let the client cancel its request
         Outgoing_Request_Cancelled (
            Queue.Item_Payload (Item_Reference_Type (Req_Queue_Head)));

         --  dequeue and read out the next item
         Queue.Dequeue (Obj.Request_Queue);
         Req_Queue_Head := Queue.Head (Obj.Request_Queue);

      end loop Req_Queue_Loop;
   end Cancel_Request_Queue;

   --
   --  Cancel_Outgoing_Request
   --
   procedure Cancel_Outgoing_Request (Obj : Object_Reference_Type)
   is
   begin
      --  abort if there is no outgoing request
      if Obj.Callee = null then
         return;
      end if;

      --  disconnect the server if it already received our request
      if Object_Reference_Type (Obj.Callee.Caller) = Obj then
         Obj.Callee.Caller := null;

      --  if our request is still in the queue of the server, remove it
      else
         Queue.Remove (
            Obj.Callee.Request_Queue,
            Obj.Request_Queue_Item'Access);
      end if;

      --  disconnect from server
      Obj.Callee := null;
   end Cancel_Outgoing_Request;

   --
   --  Announce_Request
   --
   procedure Announce_Request (
      Obj  : Object_Reference_Type;
      Node : Object_Reference_Type)
   is
   begin
      --  directly receive request if already waiting
      if Obj.State = Wait_For_Request then
         Receive_Request (Obj, Node);
         CPP_Thread.IPC_Wait_For_Request_Succeeded (Obj.Thread);
         null;

      --  enqueue request if not ready to receive it yet
      else
         Queue.Enqueue (Obj.Request_Queue, Node.Request_Queue_Item'Access);
      end if;
   end Announce_Request;

   --
   --  Receive_Request
   --
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
