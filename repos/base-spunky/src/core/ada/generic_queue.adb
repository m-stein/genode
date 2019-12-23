--
--  \brief  A generic FIFO data structure
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

package body Generic_Queue is

   --
   --  Initialized_Item_Object
   --
   function Initialized_Item_Object (Payload : Payload_Reference_Type)
   return Item_Type
   is (
      Next     => null,
      Payload  => Payload,
      Enqueued => False);

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
      Itm.Next     := null;
      Itm.Enqueued := True;

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
      Itm :        Item_Reference_Type)
   is
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
         declare
            Prev_Itm : Item_Pointer_Type := Obj.Head;
         begin
            --  search for the previous item of the one that shall be removed
            Items_Loop :
            while Prev_Itm.Next /= null and
                  Prev_Itm.Next /= Item_Pointer_Type (Itm)
            loop
               Prev_Itm := Prev_Itm.Next;
            end loop Items_Loop;

            --  abort if the item that shall be removed is not in the queue
            if Prev_Itm.Next = null then
               return;
            end if;

            --  remove item from queue
            Prev_Itm.Next := Prev_Itm.Next.Next;
            if Prev_Itm.Next = null then
               Obj.Tail := Prev_Itm;
            end if;
         end;
      end if;

      --  disconnect item from queue
      Itm.Next     := null;
      Itm.Enqueued := False;

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
         Dequeued.Next     := null;
         Dequeued.Enqueued := False;
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

   function Item_Enqueued (Itm : Item_Type)
   return Boolean
   is (Itm.Enqueued);

   function Head (Obj : Queue_Type)
   return Item_Pointer_Type
   is (Obj.Head);

   function Item_Payload (Itm : Item_Reference_Type)
   return Payload_Reference_Type
   is (Itm.Payload);

   function Item_Next (Itm : Item_Reference_Type)
   return Item_Pointer_Type
   is (Itm.Next);

end Generic_Queue;
