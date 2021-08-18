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

with Interfaces; use Interfaces;

package body CPP_IRQ_Controller_Pkg is

   procedure Initialize_IRQ_Controller_Pkg
   is
   begin
      IRQ_Controller_Pkg.Initialize_IRQ_Controller_Pkg;
   end Initialize_IRQ_Controller_Pkg;

   --
   --  IRQ_Controller_Size
   --
   function IRQ_Controller_Size (Ctrl : IRQ_Controller_Reference_Type)
   return Size_Type
   is (Ctrl'Size / 8);

   --
   --  Initialize
   --
   procedure Initialize (
      Ctrl        : IRQ_Controller_Reference_Type;
      Global_Ctrl : Address_Type)
   is
      pragma Unreferenced (Global_Ctrl);
   begin
      IRQ_Controller_Pkg.Initialize (Ctrl);
   end Initialize;

   --
   --  Take_Request
   --
   procedure Take_Request (
      Ctrl         :        IRQ_Controller_Reference_Type;
      IRQ_ID       : in out Unsigned_Type;
      IRQ_ID_Valid : in out Bool_Type)
   is
      pragma Unreferenced (Ctrl);
      Ada_IRQ_ID       : IRQ_ID_Type := IRQ_ID_Type (IRQ_ID);
      Ada_IRQ_ID_Valid : Boolean     := Bool_To_Ada (IRQ_ID_Valid);
   begin
      IRQ_Controller_Pkg.Take_Request (Ada_IRQ_ID, Ada_IRQ_ID_Valid);
      IRQ_ID := Unsigned_Type (Ada_IRQ_ID);
      IRQ_ID_Valid := Bool_From_Ada (Ada_IRQ_ID_Valid);
   end Take_Request;

   --
   --  Finish_Request
   --
   procedure Finish_Request (
      Ctrl : IRQ_Controller_Reference_Type)
   is
      pragma Unreferenced (Ctrl);
   begin
      IRQ_Controller_Pkg.Finish_Request;
   end Finish_Request;

   --
   --  Mask
   --
   procedure Mask (
      Ctrl   : IRQ_Controller_Reference_Type;
      IRQ_ID : Unsigned_Type)
   is
   begin
      IRQ_Controller_Pkg.Mask (Ctrl, IRQ_ID_Type (IRQ_ID));
   end Mask;

   --
   --  Unmask
   --
   procedure Unmask (
      Ctrl   : IRQ_Controller_Reference_Type;
      IRQ_ID : Unsigned_Type;
      CPU_ID : Unsigned_Type)
   is
      pragma Unreferenced (CPU_ID);
   begin
      IRQ_Controller_Pkg.Unmask (Ctrl, IRQ_ID_Type (IRQ_ID));
   end Unmask;

   --
   --  IRQ_Mode
   --
   procedure IRQ_Mode (
      Ctrl         : IRQ_Controller_Reference_Type;
      IRQ_ID       : Unsigned_Type;
      Trigger_Mode : Unsigned_Type;
      Polarity     : Unsigned_Type)
   is
      pragma Unreferenced (Ctrl);
   begin
      IRQ_Controller_Pkg.IRQ_Mode (
         IRQ_ID_Type (IRQ_ID), IRQ_Trigger_Mode_From_Unsigned (Trigger_Mode),
         IRQ_Polarity_From_Unsigned (Polarity));
   end IRQ_Mode;

   --
   --  Send_IPI
   --
   procedure Send_IPI (
      Ctrl    : IRQ_Controller_Reference_Type;
      CPU_Idx : Unsigned_Type)
   is
   begin
      IRQ_Controller_Pkg.Send_IPI (Ctrl, CPU_Index_Type (CPU_Idx));
   end Send_IPI;

   --
   --  Store_APIC_ID
   --
   procedure Store_APIC_ID (
      Ctrl    : IRQ_Controller_Reference_Type;
      CPU_Idx : Unsigned_Type)
   is
   begin
      IRQ_Controller_Pkg.Store_APIC_ID (Ctrl, CPU_Index_Type (CPU_Idx));
   end Store_APIC_ID;

   --
   --  Number_Of_IRQs
   --
   function Number_Of_IRQs
   return Unsigned_Type
   is (
      Unsigned_Type (IRQ_Controller_Pkg.Number_Of_IRQs));

   --
   --  Interprocessor_IRQ
   --
   function Interprocessor_IRQ
   return Unsigned_Type
   is (
      Unsigned_Type (IRQ_Controller_Pkg.Interprocessor_IRQ));

end CPP_IRQ_Controller_Pkg;
