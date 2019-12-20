pragma Ada_2012;
generic
   type Payload_Type;
   type Payload_Reference_Type is not null access all Payload_Type;
package Generic_Queue is

   type Queue_Type          is private;
   type Item_Type           is private;
   type Item_Pointer_Type   is access all Item_Type;
   type Item_Reference_Type is not null access all Item_Type;

   function Initialized_Object return Queue_Type;

   function Initialized_Item_Object (Payload : Payload_Reference_Type)
   return Item_Type;

   procedure Enqueue (
      Obj : in out Queue_Type;
      Itm :        Item_Reference_Type);

   procedure Dequeue (Obj : in out Queue_Type);

   function Head (Obj : Queue_Type)
   return Item_Pointer_Type;

   function Item_Payload (Itm : Item_Reference_Type)
   return Payload_Reference_Type;

   function Item_Next (Itm : Item_Reference_Type)
   return Item_Pointer_Type;

   --
   --  Remove
   --
   procedure Remove (
      Obj : in out Queue_Type;
      Itm :        Item_Reference_Type);

   --
   --  Empty
   --
   function Empty (Obj : Queue_Type)
   return Boolean;

   --
   --  Item_Enqueued
   --
   function Item_Enqueued (Itm : Item_Type)
   return Boolean;

private

   type Queue_Type is record
      Head : Item_Pointer_Type;
      Tail : Item_Pointer_Type;
   end record;

   type Item_Type is record
      Next     : Item_Pointer_Type;
      Payload  : Payload_Reference_Type;
      Enqueued : Boolean;
   end record;

end Generic_Queue;
