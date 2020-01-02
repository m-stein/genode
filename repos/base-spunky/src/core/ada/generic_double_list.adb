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

package body Generic_Double_List is

   --
   --  Initialize
   --
   procedure Initialize (List : out List_Type)
   is
   begin
      List.Head := null;
      List.Tail := null;
   end Initialize;

   --
   --  To_Tail
   --
   procedure To_Tail (
      List : in out List_Type;
      Itm  :        Item_Reference_Type)
   is
   begin
      if Item_Pointer_Type (Itm) = List.Head then
         Head_To_Tail (List);
      else
         if Item_Pointer_Type (Itm) = List.Tail then
            return;
         end if;
         Itm.Prev.Next := Itm.Next;
         Itm.Next.Prev := Itm.Prev;
         Itm.Prev := List.Tail;
         Itm.Next := null;
         List.Tail.Next := Item_Pointer_Type (Itm);
         List.Tail := Item_Pointer_Type (Itm);
      end if;
   end To_Tail;

   --
   --  Head_To_Tail
   --
   procedure Head_To_Tail (List : in out List_Type)
   is
   begin
      if List.Head = null or else
         List.Head = List.Tail
      then
         return;
      end if;
      List.Head.Prev := List.Tail;
      List.Tail.Next := List.Head;
      List.Head := List.Head.Next;
      List.Head.Prev := null;
      List.Tail := List.Tail.Next;
      List.Tail.Next := null;
   end Head_To_Tail;

   --
   --  Remove
   --
   procedure Remove (
      List : in out List_Type;
      Itm  :        Item_Reference_Type)
   is
   begin
      if Item_Pointer_Type (Itm) = List.Tail then
         List.Tail := Itm.Prev;
      else
         Itm.Next.Prev := Itm.Prev;
      end if;
      if Item_Pointer_Type (Itm) = List.Head then
         List.Head := Itm.Next;
      else
         Itm.Prev.Next := Itm.Next;
      end if;
   end Remove;

   --
   --  Insert_Tail
   --
   procedure Insert_Tail (
      List : in out List_Type;
      Itm  :        Item_Reference_Type)
   is
   begin
      if List.Tail /= null then
         List.Tail.Next := Item_Pointer_Type (Itm);
      else
         List.Head := Item_Pointer_Type (Itm);
      end if;
      Itm.Prev := List.Tail;
      Itm.Next := null;
      List.Tail := Item_Pointer_Type (Itm);
   end Insert_Tail;

   --
   --  Insert_Head
   --
   procedure Insert_Head (
      List : in out List_Type;
      Itm  :        Item_Reference_Type)
   is
   begin
      if List.Head /= null then
         List.Head.Prev := Item_Pointer_Type (Itm);
      else
         List.Tail := Item_Pointer_Type (Itm);
      end if;
      Itm.Next := List.Head;
      Itm.Prev := null;
      List.Head := Item_Pointer_Type (Itm);
   end Insert_Head;

   --
   --  For_Each
   --
   procedure For_Each (
      List : List_Type;
      Func : access procedure (Payload : Payload_Reference_Type))
   is
      Itm : Item_Pointer_Type := List.Head;
   begin
      while Itm /= null loop
         Func (Itm.Payload);
         Itm := Itm.Next;
      end loop;
   end For_Each;

   --
   --  Head
   --
   function Head (List : List_Type)
   return Item_Pointer_Type
   is (List.Head);

   --
   --  Item_Initialize
   --
   procedure Item_Initialize (
      Itm     : out Item_Type;
      Payload :     Payload_Reference_Type)
   is
   begin
      Itm.Next := null;
      Itm.Prev := null;
      Itm.Payload := Payload;
   end Item_Initialize;

   --
   --  Item_Next
   --
   function Item_Next (Itm : Item_Reference_Type)
   return Item_Pointer_Type
   is (Itm.Next);

   --
   --  Item_Payload
   --
   function Item_Payload (Itm : Item_Reference_Type)
   return Payload_Reference_Type
   is (Itm.Payload);

end Generic_Double_List;
