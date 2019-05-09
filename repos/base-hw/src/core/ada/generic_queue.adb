   --
   --  Generic_Queue
   --
   package body Generic_Queue is

--      --
--      --  Initialized_Item_Object
--      --
--      function Initialized_Item_Object (Payload : Payload_Reference_Type)
--      return Item_Type
--      is (
--         Next    => null,
--         Payload => Payload);

      --
      --  Initialized_Object
      --
      function Initialized_Object return Queue_Type
      is (
         Head => null,
         Tail => null);

--      --
--      --  Enqueue
--      --
--      procedure Enqueue (
--         Obj : in out Queue_Type;
--         Itm :        Item_Reference_Type)
--      is
--         Itm_Ptr : constant Item_Pointer_Type := Item_Pointer_Type (Itm);
--      begin
--         --  disconnect item from last queue
--         Itm.Next := null;
--
--         --  if queue is empty, make the item its head and tail
--         if Empty (Obj) then
--            Obj.Tail := Itm_Ptr;
--            Obj.Head := Itm_Ptr;
--
--         --  if queue is not empty, attach the item to its tail
--         else
--            Obj.Tail.Next := Itm_Ptr;
--            Obj.Tail      := Itm_Ptr;
--         end if;
--      end Enqueue;
--
--      --
--      --  Remove
--      --
--      procedure Remove (
--         Obj : in out Queue_Type;
--         Itm : Item_Reference_Type)
--      is
--         Curr_Itm : Item_Pointer_Type := Obj.Head;
--      begin
--         --  abort if queue is empty
--         if Empty (Obj) then
--            return;
--         end if;
--
--         --  if specified item is the first of the queue
--         if Obj.Head = Item_Pointer_Type (Itm) then
--            Obj.Head := Itm.Next;
--            if Obj.Head = null then
--               Obj.Tail := null;
--            end if;
--
--         --  if specified item is not the first of the queue
--         else
--
--            --  search for specified item in the queue
--            Items_Loop :
--            while Curr_Itm.Next /= null and
--                  Curr_Itm.Next /= Item_Pointer_Type (Itm)
--            loop
--               Curr_Itm := Curr_Itm.Next;
--            end loop Items_Loop;
--
--            --  abort if specified item could not be found
--            if Curr_Itm.Next = null then
--               return;
--            end if;
--
--            --  skip item in queue
--            Curr_Itm.Next := Curr_Itm.Next.Next;
--            if Curr_Itm.Next = null then
--               Obj.Tail := null;
--            end if;
--         end if;
--
--         --  disconnect item from queue
--         Itm.Next := null;
--
--      end Remove;
--
--      --
--      --  Dequeue
--      --
--      procedure Dequeue (Obj : in out Queue_Type)
--      is
--         Dequeued : constant Item_Pointer_Type := Obj.Head;
--      begin
--         --  if there is none or only one item, empty the queue
--         if Obj.Head = Obj.Tail then
--            Obj.Head := null;
--            Obj.Tail := null;
--
--         --  if the are multiple items, skip the head item
--         else
--            Obj.Head := Obj.Head.Next;
--         end if;
--
--         --  disconnect dequeued item from queue
--         if Dequeued /= null then
--            Dequeued.Next := null;
--         end if;
--      end Dequeue;
--
--      --
--      --  Empty
--      --
--      function Empty (Obj : Queue_Type)
--      return Boolean
--      is (Obj.Tail = null);
--
--      -----------------
--      --  Accessors  --
--      -----------------
--
--      function Head (Obj : Queue_Type)
--      return Payload_Pointer_Type
--      is (
--         if Obj.Head = null then null
--         else Payload_Pointer_Type (Obj.Head.Payload));
--
--      function Item_Payload (Itm : Item_Reference_Type)
--      return Payload_Reference_Type
--      is (Itm.Payload);
--
--      function Item_Next (Itm : Item_Reference_Type)
--      return Item_Pointer_Type
--      is (Itm.Next);

   end Generic_Queue;
