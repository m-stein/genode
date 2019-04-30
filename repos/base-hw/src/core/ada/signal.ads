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

   type Handler_Type            is private;
   type Receiver_Type           is private;
   type Handler_Reference_Type  is not null access all Handler_Type;
   type Receiver_Reference_Type is not null access all Receiver_Type;

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

private

   --
   --  Handler_Queue
   --
   package Handler_Queue is

      type Qbject_Type           is private;
      type Ibject_Type           is private;
      type Ibject_Pointer_Type   is access all Ibject_Type;
      type Ibject_Reference_Type is not null access all Ibject_Type;

      function Initialized_Object return Qbject_Type;

      function Initialized_Item_Object (Payload : Handler_Reference_Type)
      return Ibject_Type;

      procedure Enqueue (
         Obj : in out Qbject_Type;
         Itm :        Ibject_Reference_Type);

      procedure Dequeue (Obj : in out Qbject_Type);

      function Head (Obj : Qbject_Type) return Ibject_Pointer_Type;

      function Item_Payload (Itm : Ibject_Reference_Type)
      return Handler_Reference_Type;

      function Item_Next (Itm : Ibject_Reference_Type)
      return Ibject_Pointer_Type;

      procedure Remove (
         Obj : in out Qbject_Type;
         Itm : Ibject_Reference_Type);

   private

      type Qbject_Type is record
         Head : Ibject_Pointer_Type;
         Tail : Ibject_Pointer_Type;
      end record;

      type Ibject_Type is record
         Next    : Ibject_Pointer_Type;
         Payload : Handler_Reference_Type;
      end record;

      function Empty (Obj : Qbject_Type)
      return Boolean;

   end Handler_Queue;

   type Receiver_Pointer_Type is access all Receiver_Type;

   type Receiver_Type is record
      Handlers : Handler_Queue.Qbject_Type;
   end record;

   type Handler_Type is record
      Thread     : CPP_Thread.Object_Reference_Type;
      Queue_Item : aliased Handler_Queue.Ibject_Type;
      Receiver   : Receiver_Pointer_Type;
   end record;

end Signal;
