--
--  \brief  Peers of ssynchronous inter-process communication
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
         Rec : constant Receiver_Reference_Type :=
            Receiver_Reference_Type (Obj.Receiver);
      begin
         Handler_Queue.Remove (Rec.Handlers, Obj.Queue_Item'Access);
      end;
      Obj.Receiver := null;
   end Handler_Cancel_Waiting;

   --
   --  Handler_Queue
   --
   package body Handler_Queue is

      --
      --  Initialized_Item_Object
      --
      function Initialized_Item_Object (Payload : Handler_Reference_Type)
      return Ibject_Type
      is (
         Next    => null,
         Payload => Payload);

      --
      --  Initialized_Object
      --
      function Initialized_Object return Qbject_Type
      is (
         Head => null,
         Tail => null);

      --
      --  Enqueue
      --
      procedure Enqueue (
         Obj : in out Qbject_Type;
         Itm :        Ibject_Reference_Type)
      is
         Itm_Ptr : constant Ibject_Pointer_Type := Ibject_Pointer_Type (Itm);
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
         Obj : in out Qbject_Type;
         Itm : Ibject_Reference_Type)
      is
         Curr_Itm : Ibject_Pointer_Type := Obj.Head;
      begin
         --  abort if queue is empty
         if Empty (Obj) then
            return;
         end if;

         --  if specified item is the first of the queue
         if Obj.Head = Ibject_Pointer_Type (Itm) then
            Obj.Head := Itm.Next;
            if Obj.Head = null then
               Obj.Tail := null;
            end if;

         --  if specified item is not the first of the queue
         else

            --  search for specified item in the queue
            Items_Loop :
            while Curr_Itm.Next /= null and
                  Curr_Itm.Next /= Ibject_Pointer_Type (Itm)
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
      procedure Dequeue (Obj : in out Qbject_Type)
      is
         Dequeued : constant Ibject_Pointer_Type := Obj.Head;
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
      function Empty (Obj : Qbject_Type)
      return Boolean
      is (Obj.Tail = null);

      -----------------
      --  Accessors  --
      -----------------

      function Head (Obj : Qbject_Type)
      return Ibject_Pointer_Type
      is (Obj.Head);

      function Item_Payload (Itm : Ibject_Reference_Type)
      return Handler_Reference_Type
      is (Itm.Payload);

      function Item_Next (Itm : Ibject_Reference_Type)
      return Ibject_Pointer_Type
      is (Itm.Next);

   end Handler_Queue;

end Signal;
