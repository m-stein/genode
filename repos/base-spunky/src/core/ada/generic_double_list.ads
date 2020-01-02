--
--  \brief  Generic double-linked list data-structure
--  \author Martin stein
--  \date   2020-01-02
--

--
--  Copyright (C) 2019 Genode Labs GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

pragma Ada_2012;

generic
   type Payload_Type;
   type Payload_Reference_Type is not null access all Payload_Type;
package Generic_Double_List
is
   pragma Pure;

   type List_Type           is private;
   type Item_Type           is private;
   type Item_Pointer_Type   is access all Item_Type;
   type Item_Reference_Type is not null access all Item_Type;

   --
   --  Initialize
   --
   procedure Initialize (List : out List_Type);

   --
   --  To_Tail
   --
   procedure To_Tail (
      List : in out List_Type;
      Itm  :        Item_Reference_Type);

   --
   --  Head_To_Tail
   --
   procedure Head_To_Tail (List : in out List_Type);

   --
   --  Remove
   --
   procedure Remove (
      List : in out List_Type;
      Itm  :        Item_Reference_Type);

   --
   --  Insert_Tail
   --
   procedure Insert_Tail (
      List : in out List_Type;
      Itm  :        Item_Reference_Type);

   --
   --  Insert_Head
   --
   procedure Insert_Head (
      List : in out List_Type;
      Itm  :        Item_Reference_Type);

   --
   --  For_Each
   --
   procedure For_Each (
      List : List_Type;
      Func : access procedure (Payload : Payload_Reference_Type));

   --
   --  Head
   --
   function Head (List : List_Type)
   return Item_Pointer_Type;

   --
   --  Item_Initialize
   --
   procedure Item_Initialize (
      Itm     : out Item_Type;
      Payload :     Payload_Reference_Type);

   --
   --  Item_Next
   --
   function Item_Next (Itm : Item_Reference_Type)
   return Item_Pointer_Type;

   --
   --  Item_Payload
   --
   function Item_Payload (Itm : Item_Reference_Type)
   return Payload_Reference_Type;

private

   type List_Type is record
      Head : Item_Pointer_Type;
      Tail : Item_Pointer_Type;
   end record;

   type Item_Type is record
      Next    : Item_Pointer_Type;
      Prev    : Item_Pointer_Type;
      Payload : Payload_Reference_Type;
   end record;

end Generic_Double_List;
