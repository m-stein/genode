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

with CPP_Thread;

package Signal is

   type Imprint_Type                  is mod 2**64;
   type Number_Of_Submits_Type        is range 0 .. 2**32 - 1;
   type Handler_Type                  is private;
   type Context_Type                  is private;
   type Context_Killer_Type           is private;
   type Receiver_Type                 is private;
   type Handler_Reference_Type        is not null access all Handler_Type;
   type Context_Reference_Type        is not null access all Context_Type;
   type Receiver_Reference_Type       is not null access all Receiver_Type;
   type Context_Killer_Reference_Type is not null access all
                                            Context_Killer_Type;

   --
   --  Handler_Initialize
   --
   procedure Handler_Initialize (
      Obj  : Handler_Reference_Type;
      Thrd : CPP_Thread.Object_Reference_Type);

   --
   --  Handler_Deinitialize
   --
   procedure Handler_Deinitialize (Obj : Handler_Reference_Type);

   --
   --  Handler_Cancel_Waiting
   --
   procedure Handler_Cancel_Waiting (Obj : Handler_Reference_Type);

   --
   --  Context_Killer_Initialize
   --
   procedure Context_Killer_Initialize (
      Obj  : Context_Killer_Reference_Type;
      Thrd : CPP_Thread.Object_Reference_Type);

   --
   --  Context_Killer_Deinitialize
   --
   procedure Context_Killer_Deinitialize (Obj : Context_Killer_Reference_Type);

   --
   --  Context_Killer_Cancel_Waiting
   --
   procedure Context_Killer_Cancel_Waiting (
      Obj : Context_Killer_Reference_Type);

   --
   --  Context_Initialize
   --
   procedure Context_Initialize (
      Obj   : Context_Reference_Type;
      Recvr : Receiver_Reference_Type;
      Impr  : Imprint_Type);

   --
   --  Receiver_Initialize
   --
   procedure Receiver_Initialize (Obj : Receiver_Reference_Type);

private

   --
   --  Handler_Queue
   --
   package Handler_Queue is

      type Queue_Type           is private;
      type Item_Type           is private;
      type Item_Pointer_Type   is access all Item_Type;
      type Item_Reference_Type is not null access all Item_Type;

      function Initialized_Object return Queue_Type;

      function Initialized_Item_Object (Payload : Handler_Reference_Type)
      return Item_Type;

      procedure Enqueue (
         Obj : in out Queue_Type;
         Itm :        Item_Reference_Type);

      procedure Dequeue (Obj : in out Queue_Type);

      function Head (Obj : Queue_Type) return Item_Pointer_Type;

      function Item_Payload (Itm : Item_Reference_Type)
      return Handler_Reference_Type;

      function Item_Next (Itm : Item_Reference_Type)
      return Item_Pointer_Type;

      procedure Remove (
         Obj : in out Queue_Type;
         Itm : Item_Reference_Type);

   private

      type Queue_Type is record
         Head : Item_Pointer_Type;
         Tail : Item_Pointer_Type;
      end record;

      type Item_Type is record
         Next    : Item_Pointer_Type;
         Payload : Handler_Reference_Type;
      end record;

      function Empty (Obj : Queue_Type)
      return Boolean;

   end Handler_Queue;

   --
   --  Context_Queue
   --
   package Context_Queue is

      type Queue_Type           is private;
      type Item_Type           is private;
      type Item_Pointer_Type   is access all Item_Type;
      type Item_Reference_Type is not null access all Item_Type;

      function Initialized_Object return Queue_Type;

      function Initialized_Item_Object (Payload : Context_Reference_Type)
      return Item_Type;

      procedure Enqueue (
         Obj : in out Queue_Type;
         Itm :        Item_Reference_Type);

      procedure Dequeue (Obj : in out Queue_Type);

      function Head (Obj : Queue_Type) return Item_Pointer_Type;

      function Item_Payload (Itm : Item_Reference_Type)
      return Context_Reference_Type;

      function Item_Next (Itm : Item_Reference_Type)
      return Item_Pointer_Type;

      procedure Remove (
         Obj : in out Queue_Type;
         Itm : Item_Reference_Type);

   private

      type Queue_Type is record
         Head : Item_Pointer_Type;
         Tail : Item_Pointer_Type;
      end record;

      type Item_Type is record
         Next    : Item_Pointer_Type;
         Payload : Context_Reference_Type;
      end record;

      function Empty (Obj : Queue_Type)
      return Boolean;

   end Context_Queue;

   type Receiver_Pointer_Type       is access all Receiver_Type;
   type Context_Killer_Pointer_Type is access all Context_Killer_Type;
   type Context_Pointer_Type        is access all Context_Type;

   type Handler_Type is record
      Thread     : CPP_Thread.Object_Reference_Type;
      Queue_Item : aliased Handler_Queue.Item_Type;
      Receiver   : Receiver_Pointer_Type;
   end record;

   type Context_Killer_Type is record
      Thread  : CPP_Thread.Object_Reference_Type;
      Context : Context_Pointer_Type;
   end record;

   type Context_Type is record
      Deliver_Item  : Context_Queue.Item_Type;
      Contexts_Item : Context_Queue.Item_Type;
      Receiver      : Receiver_Reference_Type;
      Imprint       : Imprint_Type;
      Killer        : Context_Killer_Pointer_Type;
      Nr_Of_Submits : Number_Of_Submits_Type;
      Acknowledged  : Boolean;
      Killed        : Boolean;
   end record;

   type Receiver_Type is record
      Handlers : Handler_Queue.Queue_Type;
      Deliver  : Context_Queue.Queue_Type;
      Contexts : Context_Queue.Queue_Type;
   end record;

end Signal;
