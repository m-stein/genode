--
--  \brief  IRQ controller using the Intel I/O APIC and Local APIC
--  \author Martin stein
--  \date   2021-06-09
--

--
--  Copyright (C) 2021 Genode Labs GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

pragma Ada_2012;

with System;
with Interfaces;
with CPP;

use Interfaces;
use CPP;

package IRQ_Controller_Pkg is

   type Global_IRQ_Controller_Type is private;
   type Global_IRQ_Controller_Reference_Type is
      not null access all Global_IRQ_Controller_Type;

   type IRQ_Controller_Type is private;
   type IRQ_Controller_Reference_Type is
      not null access all IRQ_Controller_Type;

   --
   --  Initialize a new global IRQ controller object
   --
   procedure Initialize_Global_IRQ_Controller (
      GIC : Global_IRQ_Controller_Reference_Type);

   --
   --  Initialize
   --
   procedure Initialize (
      Ctrl : IRQ_Controller_Reference_Type;
      GIC  : Global_IRQ_Controller_Reference_Type);

   --
   --  Take_Request
   --
   procedure Take_Request (
      IRQ_ID       : in out IRQ_ID_Type;
      IRQ_ID_Valid : in out Boolean);

   --
   --  Finish_Request
   --
   procedure Finish_Request;

   --
   --  Mask
   --
   procedure Mask (
      Ctrl   : IRQ_Controller_Reference_Type;
      IRQ_ID : IRQ_ID_Type);

   --
   --  Unmask
   --
   procedure Unmask (
      Ctrl   : IRQ_Controller_Reference_Type;
      IRQ_ID : IRQ_ID_Type);

   --
   --  IRQ_Mode
   --
   procedure IRQ_Mode (
      Ctrl         : IRQ_Controller_Reference_Type;
      IRQ_ID       : IRQ_ID_Type;
      Trigger_Mode : IRQ_Trigger_Mode_Type;
      Polarity     : IRQ_Polarity_Type);

   --
   --  Send_IPI
   --
   procedure Send_IPI (
      Ctrl    : IRQ_Controller_Reference_Type;
      CPU_Idx : CPU_Index_Type);

   --
   --  Store_APIC_ID
   --
   procedure Store_APIC_ID (
      Ctrl    : IRQ_Controller_Reference_Type;
      CPU_Idx : CPU_Index_Type);

   --
   --  Number_Of_IRQs
   --
   function Number_Of_IRQs
   return Number_Of_IRQs_Type;

   --
   --  Interprocessor_IRQ
   --
   function Interprocessor_IRQ
   return IRQ_ID_Type;

private

   --
   --  FIXME
   --
   --  We have to get rid of these static values. The physical LAPIC address
   --  should be determined through the APIC base register of the CPU and then
   --  be translated to a virtiual address using the boot info (see base-hw
   --  x86_64/pic.cc).
   --
   LAPIC_Virt_Addr  : constant := 16#ffffffe030002000#;
   IOAPIC_Virt_Addr : constant := 16#ffffffe030004000#;

   IRQ_Remap_Base : constant := 48;

   PIC_Cmd_Master  : constant := 16#20#;
   PIC_Cmd_Slave   : constant := 16#a0#;
   PIC_Data_Master : constant := 16#21#;
   PIC_Data_Slave  : constant := 16#a1#;

   IOAPIC_IOWin_Virt_Addr : constant := IOAPIC_Virt_Addr + 16#10#;

   --
   --  FIXME
   --
   --  Dummy IPI value on non-SMP platform, should be removed when SMP is an
   --  aspect of CPUs only compiled where necessary.
   --
   Interprocessor_IRQ_ID : constant := 255;

   type Stored_APIC_ID_Type  is range 0 .. 255 with Size => 8;
   type IRQ_Index_Type is range 0 .. 255;
   subtype ISA_IRQ_Index_Type is IRQ_Index_Type range 0 .. 15;

   --
   --  Local APIC, identifier register
   --
   type LAPIC_ID_Reg_Type is record
      APIC_ID : Bitfield_8_Type;
   end record
   with Size => 32;

   for LAPIC_ID_Reg_Type use record
      APIC_ID at 0 range 24 .. 31;
   end record;

   LAPIC_ID_Reg : LAPIC_ID_Reg_Type
   with
      Volatile_Full_Access,
      Address => System'To_Address (LAPIC_Virt_Addr + 16#20#);

   --
   --  Local APIC, end of interrupt register (strict write)
   --
   LAPIC_EOI : Unsigned_32
   with
      Volatile_Full_Access,
      Address => System'To_Address (LAPIC_Virt_Addr + 16#b0#);

   --
   --  Local APIC, spurious interrupt vector register
   --
   type LAPIC_SVR_Reg_Type is record
      APIC_Enable : Bitfield_1_Type;
   end record
   with Size => 32;

   for LAPIC_SVR_Reg_Type use record
      APIC_Enable at 0 range 8 .. 8;
   end record;

   LAPIC_SVR_Reg : LAPIC_SVR_Reg_Type
   with
      Volatile_Full_Access,
      Address => System'To_Address (LAPIC_Virt_Addr + 16#f0#);

   --
   --  Local APIC, in-service registers
   --
   LAPIC_ISR_0_Reg : Unsigned_32
   with
      Volatile_Full_Access,
      Address => System'To_Address (LAPIC_Virt_Addr + 16#100#);

   LAPIC_ISR_1_Reg : Unsigned_32
   with
      Volatile_Full_Access,
      Address => System'To_Address (LAPIC_Virt_Addr + 16#110#);

   LAPIC_ISR_2_Reg : Unsigned_32
   with
      Volatile_Full_Access,
      Address => System'To_Address (LAPIC_Virt_Addr + 16#120#);

   LAPIC_ISR_3_Reg : Unsigned_32
   with
      Volatile_Full_Access,
      Address => System'To_Address (LAPIC_Virt_Addr + 16#130#);

   LAPIC_ISR_4_Reg : Unsigned_32
   with
      Volatile_Full_Access,
      Address => System'To_Address (LAPIC_Virt_Addr + 16#140#);

   LAPIC_ISR_5_Reg : Unsigned_32
   with
      Volatile_Full_Access,
      Address => System'To_Address (LAPIC_Virt_Addr + 16#150#);

   LAPIC_ISR_6_Reg : Unsigned_32
   with
      Volatile_Full_Access,
      Address => System'To_Address (LAPIC_Virt_Addr + 16#160#);

   LAPIC_ISR_7_Reg : Unsigned_32
   with
      Volatile_Full_Access,
      Address => System'To_Address (LAPIC_Virt_Addr + 16#170#);

   --
   --  Local APIC, interrupt command register (strict write)
   --
   type LAPIC_ICR_Low_Reg_Type is record
      Vector          : Bitfield_8_Type;
      Delivery_Status : Bitfield_1_Type;
      Level_Assert    : Bitfield_1_Type;
   end record
   with Size => 32;

   for LAPIC_ICR_Low_Reg_Type use record
      Vector          at 0 range  0 ..  7;
      Delivery_Status at 0 range 12 .. 12;
      Level_Assert    at 0 range 14 .. 14;
   end record;

   LAPIC_ICR_Low_Reg : LAPIC_ICR_Low_Reg_Type
   with
      Volatile_Full_Access,
      Address => System'To_Address (LAPIC_Virt_Addr + 16#300#);

   type LAPIC_ICR_High_Reg_Type is record
      Destination : Bitfield_8_Type;
   end record
   with Size => 32;

   for LAPIC_ICR_High_Reg_Type use record
      Destination at 0 range  24 .. 31;
   end record;

   LAPIC_ICR_High_Reg : LAPIC_ICR_High_Reg_Type
   with
      Volatile_Full_Access,
      Address => System'To_Address (LAPIC_Virt_Addr + 16#310#);

   --
   --  I/O APIC, I/O register select register
   --
   IOAPIC_IORegSel_IOAPICVer : constant := 16#1#;
   IOAPIC_IORegSel_IORedTbl  : constant := 16#10#;

   IOAPIC_IORegSel_Reg : Unsigned_32
   with
      Volatile_Full_Access,
      Address => System'To_Address (IOAPIC_Virt_Addr + 16#0#);

   --
   --  I/O APIC, I/O APIC version register
   --
   type IOAPIC_IOAPICVer_Reg_Type is record
      Max_Redirection_Entry : Bitfield_8_Type;
      APIC_Version          : Bitfield_8_Type;
   end record
   with Size => 32;

   for IOAPIC_IOAPICVer_Reg_Type use record
      APIC_Version          at 0 range  0 ..  7;
      Max_Redirection_Entry at 0 range 16 .. 23;
   end record;

   IOAPIC_IOAPICVer_Reg : IOAPIC_IOAPICVer_Reg_Type
   with
      Volatile_Full_Access,
      Address => System'To_Address (IOAPIC_IOWin_Virt_Addr);

   --
   --  I/O APIC, interrupt redirection table entry
   --
   type IOAPIC_IORedTbl_Low_Reg_Type is record
      Vector           : Bitfield_8_Type;
      Delivery_Mode    : Bitfield_3_Type;
      Destination_Mode : Bitfield_1_Type;
      Delivery_Status  : Bitfield_1_Type;
      Remote_IRR       : Bitfield_1_Type;
      Polarity         : Bitfield_1_Type;
      Trigger_Mode     : Bitfield_1_Type;
      Mask             : Bitfield_1_Type;
   end record
   with Size => 32;

   for IOAPIC_IORedTbl_Low_Reg_Type use record
      Vector           at 0 range  0 ..  7;
      Delivery_Mode    at 0 range  8 .. 10;
      Destination_Mode at 0 range 11 .. 11;
      Delivery_Status  at 0 range 12 .. 12;
      Polarity         at 0 range 13 .. 13;
      Remote_IRR       at 0 range 14 .. 14;
      Trigger_Mode     at 0 range 15 .. 15;
      Mask             at 0 range 16 .. 16;
   end record;

   IOAPIC_IORedTbl_Low_Reg : IOAPIC_IORedTbl_Low_Reg_Type
   with
      Volatile_Full_Access,
      Address => System'To_Address (IOAPIC_IOWin_Virt_Addr);

   type IOAPIC_IORedTbl_High_Reg_Type is record
      Destination_Field : Bitfield_8_Type;
   end record
   with Size => 32;

   for IOAPIC_IORedTbl_High_Reg_Type use record
      Destination_Field at 0 range 24 .. 31;
   end record;

   IOAPIC_IORedTbl_High_Reg : IOAPIC_IORedTbl_High_Reg_Type
   with
      Volatile_Full_Access,
      Address => System'To_Address (IOAPIC_IOWin_Virt_Addr);

   --
   --  Global components
   --
   type Stored_APIC_IDs_Type is array (CPU_Index_Type) of Stored_APIC_ID_Type;

   type IRQ_Mode_Type is record
      Trigger_Mode : IRQ_Trigger_Mode_Type;
      Polarity     : IRQ_Polarity_Type;
   end record;

   type IRQ_Modes_Type is array (IRQ_Index_Type) of IRQ_Mode_Type;

   type Global_IRQ_Controller_Type is record
      Stored_APIC_IDs : Stored_APIC_IDs_Type;
      IRQ_Modes       : IRQ_Modes_Type;
   end record;

   --
   --  IRQ controller
   --
   type Number_Of_IORedTbl_Regs_Type is range 1 .. 256;

   type IRQ_Controller_Type is record
      Number_Of_IORedTbl_Regs : Number_Of_IORedTbl_Regs_Type;
      GIC                     : Global_IRQ_Controller_Reference_Type;
   end record;

   --
   --  Builtin_FFS (emulates GCC's 'int __builtin_ffs (int x)')
   --
   function Builtin_FFS (Value : Unsigned_32)
   return Unsigned_32;

   --
   --  LAPIC_Get_Lowest_Bit
   --
   function LAPIC_Get_Lowest_Bit
   return Unsigned_32;

   --
   --  IOAPIC_Toggle_IRQ_Mask
   --
   procedure IOAPIC_Toggle_IRQ_Mask (
      Ctrl   : IRQ_Controller_Reference_Type;
      IRQ_ID : IRQ_ID_Type;
      Mask   : Boolean);

   --
   --  IRQ_Trigger_Mode_To_Bitfield_1
   --
   function IRQ_Trigger_Mode_To_Bitfield_1 (Mode : IRQ_Trigger_Mode_Type)
   return Bitfield_1_Type;

   --
   --  IRQ_Polarity_To_Bitfield_1
   --
   function IRQ_Polarity_To_Bitfield_1 (Polarity : IRQ_Polarity_Type)
   return Bitfield_1_Type;

end IRQ_Controller_Pkg;
