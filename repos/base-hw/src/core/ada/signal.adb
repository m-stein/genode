--
--  \brief  Peers of asynchronous inter-process communication
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

with CPP_Signal_Data;

package body Signal is

   --
   --  Handler_Initialize
   --
   procedure Handler_Initialize (
      Obj  : Handler_Reference_Type;
      Thrd : CPP_Thread.Object_Reference_Type)
   is
   begin
      Obj.all := (
         Thread     => Thrd,
         Queue_Item => Handler_Queue.Initialized_Item_Object (Obj),
         Receiver   => null);
   end Handler_Initialize;

   --
   --  Handler_Deinitialize
   --
   procedure Handler_Deinitialize (Obj : Handler_Reference_Type)
   is
   begin
      Handler_Cancel_Waiting (Obj);
   end Handler_Deinitialize;

   --
   --  Handler_Cancel_Waiting
   --
   procedure Handler_Cancel_Waiting (Obj : Handler_Reference_Type)
   is
   begin
      if Obj.Receiver = null then
         return;
      end if;
      declare
         Recvr : constant Receiver_Reference_Type :=
            Receiver_Reference_Type (Obj.Receiver);
      begin
         Handler_Queue.Remove (Recvr.Handlers, Obj.Queue_Item'Access);
      end;
      Obj.Receiver := null;
   end Handler_Cancel_Waiting;

   --
   --  Context_Killer_Initialize
   --
   procedure Context_Killer_Initialize (
      Obj  : Context_Killer_Reference_Type;
      Thrd : CPP_Thread.Object_Reference_Type)
   is
   begin
      Obj.all := (
         Thread  => Thrd,
         Context => null);
   end Context_Killer_Initialize;

   --
   --  Context_Killer_Deinitialize
   --
   procedure Context_Killer_Deinitialize (Obj : Context_Killer_Reference_Type)
   is
   begin
      Context_Killer_Cancel_Waiting (Obj);
   end Context_Killer_Deinitialize;

   --
   --  Context_Killer_Cancel_Waiting
   --
   procedure Context_Killer_Cancel_Waiting (
      Obj : Context_Killer_Reference_Type)
   is
   begin
      if Obj.Context = null then
         return;
      end if;
      declare
         Contxt : constant Context_Reference_Type :=
            Context_Reference_Type (Obj.Context);
      begin
         Contxt.Killer := null;
      end;
      Obj.Context := null;
   end Context_Killer_Cancel_Waiting;

   --
   --  Context_Initialize
   --
   procedure Context_Initialize (
      Obj   : Context_Reference_Type;
      Recvr : Receiver_Reference_Type;
      Impr  : CPP.Signal_Imprint_Type)
   is
   begin
      Obj.all := (
         Deliver_Item  => Context_Queue.Initialized_Item_Object (Obj),
         Contexts_Item => Context_Queue.Initialized_Item_Object (Obj),
         Receiver      => Recvr,
         Imprint       => Impr,
         Killer        => null,
         Nr_Of_Submits => 0,
         Acknowledged  => True,
         Killed        => False);

      Context_Queue.Enqueue (Obj.Receiver.Contexts, Obj.Contexts_Item'Access);

   end Context_Initialize;

   --
   --  Context_Deinitialize
   --
   procedure Context_Deinitialize (Obj : Context_Reference_Type)
   is
   begin
      if Obj.Killer /= null then
         declare
            Killer : constant Context_Killer_Reference_Type :=
               Context_Killer_Reference_Type (Obj.Killer);
         begin
            CPP_Thread.Signal_Context_Kill_Failed (Killer.Thread);
         end;
      end if;

      Context_Queue.Remove (Obj.Receiver.Contexts, Obj.Contexts_Item'Access);
      if Context_Queue.Item_Enqueued (Obj.Deliver_Item) then
         Context_Queue.Remove (Obj.Receiver.Deliver, Obj.Deliver_Item'Access);
      end if;

   end Context_Deinitialize;

   --
   --  Context_Deliverable
   --
   procedure Context_Deliverable (Obj : Context_Reference_Type)
   is
   begin
      --  abort if there are no submits pending
      if Obj.Nr_Of_Submits = 0 then
         return;
      end if;
      --  mark context deliverable and let receiver process its contexts
      if not Context_Queue.Item_Enqueued (Obj.Deliver_Item) then
         Context_Queue.Enqueue (Obj.Receiver.Deliver, Obj.Deliver_Item'Access);
      end if;
      Receiver_Deliver_Contexts (Obj.Receiver);
   end Context_Deliverable;

   --
   --  Context_Acknowledge
   --
   procedure Context_Acknowledge (Obj : Context_Reference_Type)
   is
   begin
      --  abort if no delivery is pending
      if Obj.Acknowledged then
         return;
      end if;
      --  if the context shall not be killed, start next delivery if any
      if not Obj.Killed then
         Obj.Acknowledged := True;
         Context_Deliverable (Obj);
         return;
      end if;
      --  the context shall be killed, this can be done now
      if Obj.Killer /= null then
         declare
            Killer : constant Context_Killer_Reference_Type :=
               Context_Killer_Reference_Type (Obj.Killer);
         begin
            Killer.Context := null;
            CPP_Thread.Signal_Context_Kill_Done (Killer.Thread);
            Obj.Killer := null;
         end;
      end if;
   end Context_Acknowledge;

   --
   --  Context_Can_Kill
   --
   function Context_Can_Kill (Obj : Context_Type)
   return Boolean
   is (not Obj.Killed or (Obj.Killed and Obj.Acknowledged));

   --
   --  Context_Kill
   --
   procedure Context_Kill (
      Obj    : Context_Reference_Type;
      Killer : Context_Killer_Reference_Type)
   is
   begin
      --  check if in a kill operation or already killed
      if Obj.Killed then
         return;
      end if;
      --  kill directly if there is no unacknowledged delivery
      if Obj.Acknowledged then
         Obj.Killed := True;
         return;
      end if;
      --  wait for the missing delivery acknowledgement
      Obj.Killer := Context_Killer_Pointer_Type (Killer);
      Obj.Killed := True;
      Killer.Context := Context_Pointer_Type (Obj);
      CPP_Thread.Signal_Context_Kill_Pending (Killer.Thread);
      return;
   end Context_Kill;

   --
   --  Receiver_Initialize
   --
   procedure Receiver_Initialize (Obj : Receiver_Reference_Type)
   is
   begin
      Obj.all := (
         Handlers => Handler_Queue.Initialized_Object,
         Deliver  => Context_Queue.Initialized_Object,
         Contexts => Context_Queue.Initialized_Object);
   end Receiver_Initialize;

   --
   --  Receiver_Deliver_Contexts
   --
   procedure Receiver_Deliver_Contexts (Obj : Receiver_Reference_Type)
   is
   begin
      --  deliver as long as we have waiting handlers and deliverable contexts
      Deliver_Contexts :
      while
         not (
            Context_Queue.Empty (Obj.Deliver) or
            Handler_Queue.Empty (Obj.Handlers))
      loop
         declare
            --  select a waiting handler and a deliverable context
            use Context_Queue;
            use Handler_Queue;
            Context : constant Context_Reference_Type :=
               Context_Reference_Type (Context_Queue.Head (Obj.Deliver));
            Handler : constant Handler_Reference_Type :=
               Handler_Reference_Type (Handler_Queue.Head (Obj.Handlers));

            --  create a signal-delivery data from the context information
            Data : aliased constant CPP_Signal_Data.Object_Type :=
               CPP_Signal_Data.Initialized_Object (
                  Context.Imprint,
                  CPP.Signal_Number_Of_Submits_Type (Context.Nr_Of_Submits));
         begin
            --  update receiver and let thread receive the data
            Handler.Receiver := null;
            CPP_Thread.Signal_Receive (Handler.Thread, Data);

            --  update context
            Context.Nr_Of_Submits := 0;
            Context.Acknowledged := False;

            --  remove context and handler from the waiting queues
            Context_Queue.Dequeue (Obj.Deliver);
            Handler_Queue.Dequeue (Obj.Handlers);
         end;
      end loop Deliver_Contexts;
   end Receiver_Deliver_Contexts;

   --
   --  Handler_Queue
   --
   package body Handler_Queue is

      --
      --  Initialized_Item_Object
      --
      function Initialized_Item_Object (Payload : Handler_Reference_Type)
      return Item_Type
      is (
         Next    => null,
         Payload => Payload);

      --
      --  Initialized_Object
      --
      function Initialized_Object return Queue_Type
      is (
         Head => null,
         Tail => null);

      --
      --  Enqueue
      --
      procedure Enqueue (
         Obj : in out Queue_Type;
         Itm :        Item_Reference_Type)
      is
         Itm_Ptr : constant Item_Pointer_Type := Item_Pointer_Type (Itm);
      begin
         --  disconnect item from last queue
         Itm.Next := null;

         --  if queue is empty, make the item its head and tail
         if Empty (Obj) then
            Obj.Tail := Itm_Ptr;
            Obj.Head := Itm_Ptr;

         --  if queue is not empty, attach the item to its tail
         else
            Obj.Tail.Next := Itm_Ptr;
            Obj.Tail      := Itm_Ptr;
         end if;
      end Enqueue;

      --
      --  Remove
      --
      procedure Remove (
         Obj : in out Queue_Type;
         Itm : Item_Reference_Type)
      is
         Curr_Itm : Item_Pointer_Type := Obj.Head;
      begin
         --  abort if queue is empty
         if Empty (Obj) then
            return;
         end if;

         --  if specified item is the first of the queue
         if Obj.Head = Item_Pointer_Type (Itm) then
            Obj.Head := Itm.Next;
            if Obj.Head = null then
               Obj.Tail := null;
            end if;

         --  if specified item is not the first of the queue
         else

            --  search for specified item in the queue
            Items_Loop :
            while Curr_Itm.Next /= null and
                  Curr_Itm.Next /= Item_Pointer_Type (Itm)
            loop
               Curr_Itm := Curr_Itm.Next;
            end loop Items_Loop;

            --  abort if specified item could not be found
            if Curr_Itm.Next = null then
               return;
            end if;

            --  skip item in queue
            Curr_Itm.Next := Curr_Itm.Next.Next;
            if Curr_Itm.Next = null then
               Obj.Tail := null;
            end if;
         end if;

         --  disconnect item from queue
         Itm.Next := null;

      end Remove;

      --
      --  Dequeue
      --
      procedure Dequeue (Obj : in out Queue_Type)
      is
         Dequeued : constant Item_Pointer_Type := Obj.Head;
      begin
         --  if there is none or only one item, empty the queue
         if Obj.Head = Obj.Tail then
            Obj.Head := null;
            Obj.Tail := null;

         --  if the are multiple items, skip the head item
         else
            Obj.Head := Obj.Head.Next;
         end if;

         --  disconnect dequeued item from queue
         if Dequeued /= null then
            Dequeued.Next := null;
         end if;
      end Dequeue;

      --
      --  Empty
      --
      function Empty (Obj : Queue_Type)
      return Boolean
      is (Obj.Tail = null);

      -----------------
      --  Accessors  --
      -----------------

      function Head (Obj : Queue_Type)
      return Handler_Pointer_Type
      is (
         if Obj.Head = null then null
         else Handler_Pointer_Type (Obj.Head.Payload));

      function Item_Payload (Itm : Item_Reference_Type)
      return Handler_Reference_Type
      is (Itm.Payload);

      function Item_Next (Itm : Item_Reference_Type)
      return Item_Pointer_Type
      is (Itm.Next);

   end Handler_Queue;

   --
   --  Context_Queue
   --
   package body Context_Queue is

      --
      --  Initialized_Item_Object
      --
      function Initialized_Item_Object (Payload : Context_Reference_Type)
      return Item_Type
      is (
         Next    => null,
         Payload => Payload);

      --
      --  Initialized_Object
      --
      function Initialized_Object return Queue_Type
      is (
         Head => null,
         Tail => null);

      --
      --  Enqueue
      --
      procedure Enqueue (
         Obj : in out Queue_Type;
         Itm :        Item_Reference_Type)
      is
         Itm_Ptr : constant Item_Pointer_Type := Item_Pointer_Type (Itm);
      begin
         --  disconnect item from last queue
         Itm.Next := null;

         --  if queue is empty, make the item its head and tail
         if Empty (Obj) then
            Obj.Tail := Itm_Ptr;
            Obj.Head := Itm_Ptr;

         --  if queue is not empty, attach the item to its tail
         else
            Obj.Tail.Next := Itm_Ptr;
            Obj.Tail      := Itm_Ptr;
         end if;
      end Enqueue;

      --
      --  Remove
      --
      procedure Remove (
         Obj : in out Queue_Type;
         Itm : Item_Reference_Type)
      is
         Curr_Itm : Item_Pointer_Type := Obj.Head;
      begin
         --  abort if queue is empty
         if Empty (Obj) then
            return;
         end if;

         --  if specified item is the first of the queue
         if Obj.Head = Item_Pointer_Type (Itm) then
            Obj.Head := Itm.Next;
            if Obj.Head = null then
               Obj.Tail := null;
            end if;

         --  if specified item is not the first of the queue
         else

            --  search for specified item in the queue
            Items_Loop :
            while Curr_Itm.Next /= null and
                  Curr_Itm.Next /= Item_Pointer_Type (Itm)
            loop
               Curr_Itm := Curr_Itm.Next;
            end loop Items_Loop;

            --  abort if specified item could not be found
            if Curr_Itm.Next = null then
               return;
            end if;

            --  skip item in queue
            Curr_Itm.Next := Curr_Itm.Next.Next;
            if Curr_Itm.Next = null then
               Obj.Tail := null;
            end if;
         end if;

         --  disconnect item from queue
         Itm.Next := null;

      end Remove;

      --
      --  Dequeue
      --
      procedure Dequeue (Obj : in out Queue_Type)
      is
         Dequeued : constant Item_Pointer_Type := Obj.Head;
      begin
         --  if there is none or only one item, empty the queue
         if Obj.Head = Obj.Tail then
            Obj.Head := null;
            Obj.Tail := null;

         --  if the are multiple items, skip the head item
         else
            Obj.Head := Obj.Head.Next;
         end if;

         --  disconnect dequeued item from queue
         if Dequeued /= null then
            Dequeued.Next := null;
         end if;
      end Dequeue;

      --
      --  Empty
      --
      function Empty (Obj : Queue_Type)
      return Boolean
      is (Obj.Tail = null);

      --
      --  Item_Enqueued
      --
      function Item_Enqueued (Itm : Item_Type)
      return Boolean
      is (Itm.Next /= null);

      -----------------
      --  Accessors  --
      -----------------

      function Head (Obj : Queue_Type)
      return Context_Pointer_Type
      is (
         if Obj.Head = null then null
         else Context_Pointer_Type (Obj.Head.Payload));

      function Item_Payload (Itm : Item_Reference_Type)
      return Context_Reference_Type
      is (Itm.Payload);

      function Item_Next (Itm : Item_Reference_Type)
      return Item_Pointer_Type
      is (Itm.Next);

   end Context_Queue;

end Signal;
