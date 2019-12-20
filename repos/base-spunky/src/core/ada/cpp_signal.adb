--
--  \brief  Glue code between Ada and C++ interfaces of signalling classes
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

package body CPP_Signal is

   --
   --  Handler_Size
   --
   function Handler_Size (Obj : Signal.Handler_Type)
   return CPP.Uint32_Type
   is (CPP.Uint32_Type (Obj'Size / 8));

   --
   --  Handler_Initialize
   --
   procedure Handler_Initialize (
      Obj  : Signal.Handler_Reference_Type;
      Thrd : CPP_Thread.Object_Reference_Type)
   is
   begin
      Signal.Handler_Initialize (Obj, Thrd);
   end Handler_Initialize;

   --
   --  Handler_Deinitialize
   --
   procedure Handler_Deinitialize (Obj : Signal.Handler_Reference_Type)
   is
   begin
      Signal.Handler_Deinitialize (Obj);
   end Handler_Deinitialize;

   --
   --  Handler_Cancel_Waiting
   --
   procedure Handler_Cancel_Waiting (Obj : Signal.Handler_Reference_Type)
   is
   begin
      Signal.Handler_Cancel_Waiting (Obj);
   end Handler_Cancel_Waiting;

   --
   --  Receiver_Size
   --
   function Receiver_Size (Obj : Signal.Receiver_Type)
   return CPP.Uint32_Type
   is (CPP.Uint32_Type (Obj'Size / 8));

   --
   --  Receiver_Initialize
   --
   procedure Receiver_Initialize (Obj : Signal.Receiver_Reference_Type)
   is
   begin
      Signal.Receiver_Initialize (Obj);
   end Receiver_Initialize;

   --
   --  Receiver_Deinitialize
   --
   procedure Receiver_Deinitialize (Obj : in out Signal.Receiver_Type)
   is
   begin
      Signal.Receiver_Deinitialize (Obj);
   end Receiver_Deinitialize;

   --
   --  Receiver_Can_Add_Handler
   --
   function Receiver_Can_Add_Handler (
      Obj     : Signal.Receiver_Type;
      Handler : Signal.Handler_Type)
   return CPP.Bool_Type
   is (CPP.Bool_From_Ada (Signal.Receiver_Can_Add_Handler (Obj, Handler)));

   --
   --  Receiver_Add_Handler
   --
   procedure Receiver_Add_Handler (
      Obj     : Signal.Receiver_Reference_Type;
      Handler : Signal.Handler_Reference_Type)
   is
   begin
      Signal.Receiver_Add_Handler (Obj, Handler);
   end Receiver_Add_Handler;

   --
   --  Context_Killer_Size
   --
   function Context_Killer_Size (Obj : Signal.Context_Killer_Type)
   return CPP.Uint32_Type
   is (CPP.Uint32_Type (Obj'Size / 8));

   --
   --  Context_Killer_Initialize
   --
   procedure Context_Killer_Initialize (
      Obj  : Signal.Context_Killer_Reference_Type;
      Thrd : CPP_Thread.Object_Reference_Type)
   is
   begin
      Signal.Context_Killer_Initialize (Obj, Thrd);
   end Context_Killer_Initialize;

   --
   --  Context_Killer_Deinitialize
   --
   procedure Context_Killer_Deinitialize (
      Obj : Signal.Context_Killer_Reference_Type)
   is
   begin
      Signal.Context_Killer_Deinitialize (Obj);
   end Context_Killer_Deinitialize;

   --
   --  Context_Killer_Cancel_Waiting
   --
   procedure Context_Killer_Cancel_Waiting (
      Obj : Signal.Context_Killer_Reference_Type)
   is
   begin
      Signal.Context_Killer_Cancel_Waiting (Obj);
   end Context_Killer_Cancel_Waiting;

   --
   --  Context_Size
   --
   function Context_Size (Obj : Signal.Context_Type)
   return CPP.Uint32_Type
   is (CPP.Uint32_Type (Obj'Size / 8));

   --
   --  Context_Initialize
   --
   procedure Context_Initialize (
      Obj   : Signal.Context_Reference_Type;
      Recvr : Signal.Receiver_Reference_Type;
      Impr  : CPP.Signal_Imprint_Type)
   is
   begin
      Signal.Context_Initialize (Obj, Recvr, Impr);
   end Context_Initialize;

   --
   --  Context_Deinitialize
   --
   procedure Context_Deinitialize (Obj : Signal.Context_Reference_Type)
   is
   begin
      Signal.Context_Deinitialize (Obj);
   end Context_Deinitialize;

   --
   --  Context_Can_Kill
   --
   function Context_Can_Kill (Obj : Signal.Context_Type)
   return CPP.Bool_Type
   is (CPP.Bool_From_Ada (Signal.Context_Can_Kill (Obj)));

   --
   --  Context_Kill
   --
   procedure Context_Kill (
      Obj    : Signal.Context_Reference_Type;
      Killer : Signal.Context_Killer_Reference_Type)
   is
   begin
      Signal.Context_Kill (Obj, Killer);
   end Context_Kill;

   --
   --  Context_Acknowledge
   --
   procedure Context_Acknowledge (Obj : Signal.Context_Reference_Type)
   is
   begin
      Signal.Context_Acknowledge (Obj);
   end Context_Acknowledge;

   --
   --  Context_Can_Submit
   --
   function Context_Can_Submit (
      Obj           : Signal.Context_Type;
      Nr_Of_Submits : CPP.Signal_Number_Of_Submits_Type)
   return CPP.Bool_Type
   is (
      CPP.Bool_From_Ada (
         Signal.Context_Can_Submit (
            Obj, Signal.Number_Of_Submits_Type (Nr_Of_Submits))));

   --
   --  Context_Submit
   --
   procedure Context_Submit (
      Obj           : Signal.Context_Reference_Type;
      Nr_Of_Submits : CPP.Signal_Number_Of_Submits_Type)
   is
   begin
      Signal.Context_Submit (
         Obj, Signal.Number_Of_Submits_Type (Nr_Of_Submits));
   end Context_Submit;

end CPP_Signal;
