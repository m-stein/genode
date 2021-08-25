--
--  \brief  Glue code between Ada and C++ interface of the IRQ Controller
--  \author Martin stein
--  \date   2021-06-03
--

--
--  Copyright (C) 2021 Genode Labs GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

pragma Ada_2012;

with CPP;                use CPP;
with CPP_Architecture;   use CPP_Architecture;
with IRQ_Controller_Pkg; use IRQ_Controller_Pkg;

package CPP_IRQ_Controller_Pkg is

   function Global_IRQ_Controller_Size (
      GIC : Global_IRQ_Controller_Type)
   return Size_Type
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZNK6Kernel15Opaque_ada_typeIN5Board27" &
         "Global_interrupt_controllerELm544EE9_ada_sizeEv";

   procedure Initialize_Global_IRQ_Controller (
      GIC : Global_IRQ_Controller_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN5Board27Global_interrupt_controllerC1Ev";

   --
   --  IRQ_Controller_Size
   --
   function IRQ_Controller_Size (Ctrl : IRQ_Controller_Type)
   return Size_Type
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZNK6Kernel15Opaque_ada_typeIN5Board" &
         "26Local_interrupt_controllerELm16EE9_ada_sizeEv";

   --
   --  Initialize
   --
   procedure Initialize (
      Ctrl : IRQ_Controller_Reference_Type;
      GIC  : Global_IRQ_Controller_Reference_Type)
   with Export,
        Convention    => C,
        External_Name =>
           "_ZN5Board26Local_interrupt_controller" &
           "C1ERNS_27Global_interrupt_controllerE";

   --
   --  Take_Request
   --
   procedure Take_Request (
      Ctrl         :        IRQ_Controller_Reference_Type;
      IRQ_ID       : in out Unsigned_Type;
      IRQ_ID_Valid : in out Bool_Type)
   with Export,
        Convention    => C,
        External_Name =>
           "_ZN5Board26Local_interrupt_controller12take_requestERjRb";

   --
   --  Finish_Request
   --
   procedure Finish_Request (
      Ctrl : IRQ_Controller_Reference_Type)
   with Export,
        Convention    => C,
        External_Name =>
           "_ZN5Board26Local_interrupt_controller14finish_requestEv";

   --
   --  Mask
   --
   procedure Mask (
      Ctrl   : IRQ_Controller_Reference_Type;
      IRQ_ID : Unsigned_Type)
   with Export,
        Convention    => C,
        External_Name => "_ZN5Board26Local_interrupt_controller4maskEj";

   --
   --  Unmask
   --
   procedure Unmask (
      Ctrl   : IRQ_Controller_Reference_Type;
      IRQ_ID : Unsigned_Type;
      CPU_ID : Unsigned_Type)
   with Export,
        Convention    => C,
        External_Name => "_ZN5Board26Local_interrupt_controller6unmaskEjj";

   --
   --  IRQ_Mode
   --
   procedure IRQ_Mode (
      Ctrl         : IRQ_Controller_Reference_Type;
      IRQ_ID       : Unsigned_Type;
      Trigger_Mode : Unsigned_Type;
      Polarity     : Unsigned_Type)
   with Export,
        Convention    => C,
        External_Name => "_ZN5Board26Local_interrupt_controller8irq_modeEjjj";

   --
   --  Send_IPI
   --
   procedure Send_IPI (
      Ctrl    : IRQ_Controller_Reference_Type;
      CPU_Idx : Unsigned_Type)
   with Export,
        Convention    => C,
        External_Name => "_ZNK5Board26Local_interrupt_controller8send_ipiEj";

   --
   --  Store_APIC_ID
   --
   procedure Store_APIC_ID (
      Ctrl    : IRQ_Controller_Reference_Type;
      CPU_Idx : Unsigned_Type)
   with Export,
        Convention    => C,
        External_Name =>
           "_ZN5Board26Local_interrupt_controller13store_apic_idEj";

   --
   --  Number_Of_IRQs
   --
   function Number_Of_IRQs
   return Unsigned_Type
   with Export,
        Convention    => C,
        External_Name => "_ZN5Board26Local_interrupt_controller10nr_of_irqsEv";

   --
   --  Interprocessor_IRQ
   --
   function Interprocessor_IRQ
   return Unsigned_Type
   with Export,
        Convention    => C,
        External_Name => "_ZN5Board26Local_interrupt_controller3ipiEv";

end CPP_IRQ_Controller_Pkg;
