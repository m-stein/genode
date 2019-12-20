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

with CPP_Signal;

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
      if Obj.Receiver /= null then
         Handler_Queue.Remove (
            Obj.Receiver.Handlers, Obj.Queue_Item'Access);
         Obj.Receiver := null;
      end if;
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
   --  Context_Can_Submit
   --
   function Context_Can_Submit (
      Obj           : Context_Type;
      Nr_Of_Submits : Number_Of_Submits_Type)
   return Boolean
   is (
      not (
         Obj.Killed or
         Obj.Nr_Of_Submits > Number_Of_Submits_Type'Last - Nr_Of_Submits));

   --
   --  Context_Submit
   --
   procedure Context_Submit (
      Obj           : Context_Reference_Type;
      Nr_Of_Submits : Number_Of_Submits_Type)
   is
   begin
      Obj.Nr_Of_Submits := Obj.Nr_Of_Submits + Nr_Of_Submits;
      if Obj.Acknowledged then
         Context_Deliverable (Obj);
      end if;
   end Context_Submit;

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
            Context : constant Context_Reference_Type :=
               Context_Reference_Type (
               Context_Queue.Item_Payload (
               Context_Queue.Item_Reference_Type (
               Context_Queue.Head (Obj.Deliver))));
            Handler : constant Handler_Reference_Type :=
               Handler_Reference_Type (
               Handler_Queue.Item_Payload (
               Handler_Queue.Item_Reference_Type (
               Handler_Queue.Head (Obj.Handlers))));
         begin
            --  update receiver and let thread receive the data
            Handler.Receiver := null;
            CPP_Thread.Signal_Receive_Signal (
               Handler.Thread,
               Context.Imprint,
               CPP.Signal_Number_Of_Submits_Type (Context.Nr_Of_Submits));

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
   --  Receiver_Can_Add_Handler
   --
   function Receiver_Can_Add_Handler (
      Obj     : Receiver_Type;
      Handler : Handler_Type)
   return Boolean
   is (
      Handler.Receiver = null);

   --
   --  Receiver_Add_Handler
   --
   procedure Receiver_Add_Handler (
      Obj     : Receiver_Reference_Type;
      Handler : Handler_Reference_Type)
   is
   begin
      Handler_Queue.Enqueue (Obj.Handlers, Handler.Queue_Item'Access);
      Handler.Receiver := Receiver_Pointer_Type (Obj);
      CPP_Thread.Signal_Wait_For_Signal (Handler.Thread);
      Receiver_Deliver_Contexts (Obj);
   end Receiver_Add_Handler;

   --
   --  Receiver_Deinitialize
   --
   procedure Receiver_Deinitialize (Obj : in out Receiver_Type)
   is
   begin
      --  destruct all contexts in the context queue
      Destruct_Contexts :
      while not Context_Queue.Empty (Obj.Contexts) loop
         declare
            --  read head of context queue
            Context : constant Context_Reference_Type :=
               Context_Reference_Type (
               Context_Queue.Item_Payload (
               Context_Queue.Item_Reference_Type (
               Context_Queue.Head (Obj.Contexts))));
         begin
            --  dequeue and destruct head of context queue
            Context_Queue.Dequeue (Obj.Contexts);
            CPP_Signal.Context_Destruct (Context);
         end;
      end loop Destruct_Contexts;
   end Receiver_Deinitialize;

end Signal;
