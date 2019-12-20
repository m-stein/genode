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

with CPP_Thread;
with CPP;
with Generic_Queue;

package Signal is

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
      Impr  : CPP.Signal_Imprint_Type);

   --
   --  Context_Deinitialize
   --
   procedure Context_Deinitialize (Obj : Context_Reference_Type);

   --
   --  Context_Can_Kill
   --
   function Context_Can_Kill (Obj : Context_Type)
   return Boolean;

   --
   --  Context_Kill
   --
   procedure Context_Kill (
      Obj    : Context_Reference_Type;
      Killer : Context_Killer_Reference_Type);

   --
   --  Context_Acknowledge
   --
   procedure Context_Acknowledge (Obj : Context_Reference_Type);

   --
   --  Context_Can_Submit
   --
   function Context_Can_Submit (
      Obj           : Context_Type;
      Nr_Of_Submits : Number_Of_Submits_Type)
   return Boolean;

   --
   --  Context_Submit
   --
   procedure Context_Submit (
      Obj           : Context_Reference_Type;
      Nr_Of_Submits : Number_Of_Submits_Type);

   --
   --  Receiver_Initialize
   --
   procedure Receiver_Initialize (Obj : Receiver_Reference_Type);

   --
   --  Receiver_Deinitialize
   --
   procedure Receiver_Deinitialize (Obj : in out Receiver_Type);

   --
   --  Receiver_Can_Add_Handler
   --
   function Receiver_Can_Add_Handler (
      Obj     : Receiver_Type;
      Handler : Handler_Type)
   return Boolean;

   --
   --  Receiver_Add_Handler
   --
   procedure Receiver_Add_Handler (
      Obj     : Receiver_Reference_Type;
      Handler : Handler_Reference_Type);

private

   type Receiver_Pointer_Type       is access all Receiver_Type;
   type Context_Killer_Pointer_Type is access all Context_Killer_Type;
   type Handler_Pointer_Type        is access all Handler_Type;
   type Context_Pointer_Type        is access all Context_Type;

   package Handler_Queue is new Generic_Queue (
      Handler_Type,
      Handler_Reference_Type);

   package Context_Queue is new Generic_Queue (
      Context_Type,
      Context_Reference_Type);

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
      Deliver_Item  : aliased Context_Queue.Item_Type;
      Contexts_Item : aliased Context_Queue.Item_Type;
      Receiver      : Receiver_Reference_Type;
      Imprint       : CPP.Signal_Imprint_Type;
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

   --
   --  Context_Deliverable
   --
   procedure Context_Deliverable (Obj : Context_Reference_Type);

   --
   --  Receiver_Deliver_Contexts
   --
   procedure Receiver_Deliver_Contexts (Obj : Receiver_Reference_Type);

end Signal;
