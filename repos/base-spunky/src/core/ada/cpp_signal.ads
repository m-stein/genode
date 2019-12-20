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

with Signal;
with CPP;
with CPP_Thread;

package CPP_Signal is

   --
   --  Handler_Size
   --
   function Handler_Size (Obj : Signal.Handler_Type)
   return CPP.Uint32_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel11object_sizeERKNS_14Signal_handlerE";

   --
   --  Handler_Initialize
   --
   procedure Handler_Initialize (
      Obj  : Signal.Handler_Reference_Type;
      Thrd : CPP_Thread.Object_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel14Signal_handlerC1ERNS_6ThreadE";

   --
   --  Handler_Deinitialize
   --
   procedure Handler_Deinitialize (Obj : Signal.Handler_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel14Signal_handlerD1Ev";

   --
   --  Handler_Cancel_Waiting
   --
   procedure Handler_Cancel_Waiting (Obj : Signal.Handler_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel14Signal_handler14cancel_waitingEv";

   --
   --  Receiver_Size
   --
   function Receiver_Size (Obj : Signal.Receiver_Type)
   return CPP.Uint32_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel11object_sizeERKNS_15Signal_receiverE";

   --
   --  Receiver_Initialize
   --
   procedure Receiver_Initialize (Obj : Signal.Receiver_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel15Signal_receiver10initializeEv";

   --
   --  Receiver_Deinitialize
   --
   procedure Receiver_Deinitialize (Obj : in out Signal.Receiver_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel15Signal_receiver12deinitializeEv";

   --
   --  Receiver_Can_Add_Handler
   --
   function Receiver_Can_Add_Handler (
      Obj     : Signal.Receiver_Type;
      Handler : Signal.Handler_Type)
   return CPP.Bool_Type
   with
      Export,
      Convention    => C,
      External_Name =>
        "_ZNK6Kernel15Signal_receiver15can_add_handlerERKNS_14Signal_handlerE";

   --
   --  Receiver_Add_Handler
   --
   procedure Receiver_Add_Handler (
      Obj     : Signal.Receiver_Reference_Type;
      Handler : Signal.Handler_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN6Kernel15Signal_receiver11add_handlerERNS_14Signal_handlerE";

   --
   --  Context_Killer_Size
   --
   function Context_Killer_Size (Obj : Signal.Context_Killer_Type)
   return CPP.Uint32_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel11object_sizeERKNS_21Signal_context_killerE";

   --
   --  Context_Killer_Initialize
   --
   procedure Context_Killer_Initialize (
      Obj  : Signal.Context_Killer_Reference_Type;
      Thrd : CPP_Thread.Object_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel21Signal_context_killerC1ERNS_6ThreadE";

   --
   --  Context_Killer_Deinitialize
   --
   procedure Context_Killer_Deinitialize (
      Obj : Signal.Context_Killer_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel21Signal_context_killerD1Ev";

   --
   --  Context_Killer_Cancel_Waiting
   --
   procedure Context_Killer_Cancel_Waiting (
      Obj : Signal.Context_Killer_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel21Signal_context_killer14cancel_waitingEv";

   --
   --  Context_Size
   --
   function Context_Size (Obj : Signal.Context_Type)
   return CPP.Uint32_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel11object_sizeERKNS_14Signal_contextE";

   --
   --  Context_Initialize
   --
   procedure Context_Initialize (
      Obj   : Signal.Context_Reference_Type;
      Recvr : Signal.Receiver_Reference_Type;
      Impr  : CPP.Signal_Imprint_Type)
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN6Kernel14Signal_context10initializeERNS_15Signal_receiverEm";

   --
   --  Context_Deinitialize
   --
   procedure Context_Deinitialize (Obj : Signal.Context_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel14Signal_context12deinitializeEv";

   --
   --  Context_Destruct
   --
   procedure Context_Destruct (Obj : Signal.Context_Reference_Type)
   with
      Import,
      Convention    => C,
      External_Name => "_ZN6Kernel14Signal_contextD1Ev";

   --
   --  Context_Can_Kill
   --
   function Context_Can_Kill (Obj : Signal.Context_Type)
   return CPP.Bool_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZNK6Kernel14Signal_context8can_killEv";

   --
   --  Context_Kill
   --
   procedure Context_Kill (
      Obj    : Signal.Context_Reference_Type;
      Killer : Signal.Context_Killer_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN6Kernel14Signal_context4killERNS_21Signal_context_killerE";

   --
   --  Context_Acknowledge
   --
   procedure Context_Acknowledge (Obj : Signal.Context_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel14Signal_context3ackEv";

   --
   --  Context_Can_Submit
   --
   function Context_Can_Submit (
      Obj           : Signal.Context_Type;
      Nr_Of_Submits : CPP.Signal_Number_Of_Submits_Type)
   return CPP.Bool_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZNK6Kernel14Signal_context10can_submitEj";

   --
   --  Context_Submit
   --
   procedure Context_Submit (
      Obj           : Signal.Context_Reference_Type;
      Nr_Of_Submits : CPP.Signal_Number_Of_Submits_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel14Signal_context6submitEj";

end CPP_Signal;
