--
--  \brief  Timer device driver using the Intel PIT and Local APIC
--  \author Martin stein
--  \date   2020-06-09
--

--
--  Copyright (C) 2021 Genode Labs GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

pragma Ada_2012;

with System;
with CPP;

use CPP;

package Timer_Device is

   type Timer_Device_Type is private;
   type Ticks_Type is range 0 .. 2**32 - 1;

   --
   --  Initialize
   --
   procedure Initialize (Device : out Timer_Device_Type);

   --
   --  Ticks_To_US
   --
   function Ticks_To_US (
      Device : Timer_Device_Type;
      Ticks  : Ticks_Type)
   return Time_Type;

   --
   --  Duration
   --
   function Duration (
      Device                : Timer_Device_Type;
      Last_Timeout_Duration : Time_Type)
   return Time_Type;

   --
   --  Start_One_Shot
   --
   procedure Start_One_Shot (Ticks : Time_Type);

   --
   --  US_To_Ticks
   --
   function US_To_Ticks (
      Device : Timer_Device_Type;
      US     : Time_Type)
   return Time_Type;

   --
   --  Interrupt_ID
   --
   function Interrupt_ID
   return IRQ_ID_Type;

private

   --
   --  FIXME
   --
   --  We have to get rid of these static values. The physical LAPIC address
   --  should be determined through the APIC base register of the CPU and then
   --  be translated to a virtiual address using the boot info (see base-hw
   --  x86_64/pit.cc).
   --
   LAPIC_Phys_Base : constant := 16#fee00000#;
   LAPIC_Virt_Base : constant := 16#ffffffe030002000#;

   LAPIC_LVT_Timer_Entry_Addr         : constant := LAPIC_Virt_Base + 16#320#;
   LAPIC_Timer_Divide_Config_Reg_Addr : constant := LAPIC_Virt_Base + 16#3e0#;

   Min_Ticks_Per_MS   : constant := 1000;
   PIT_Channel_2_Gate : constant := 16#61#;
   PIT_Channel_2_Data : constant := 16#42#;
   PIT_Channel_0_Data : constant := 16#40#;
   PIT_Mode           : constant := 16#43#;
   PIT_Ticks_Per_Sec  : constant := 1193182;
   PIT_Sleep_MS       : constant := 50;
   PIT_Sleep_Ticks    : constant := (PIT_Ticks_Per_Sec / 1000) * PIT_Sleep_MS;

   type Divide_Configuration_Type is range 1 .. 6;

   type Divide_Configuration_Register_Type is record
      Bits_0_To_1 : Natural range 0 .. 2**2 - 1;
      Bits_2_To_2 : Natural range 0 .. 2**1 - 1;
   end record
   with Size => 32, Volatile_Full_Access;

   for Divide_Configuration_Register_Type use
   record
      Bits_0_To_1 at 0 range 0 .. 1;
      Bits_2_To_2 at 0 range 3 .. 3;
   end record;

   type LVT_Entry_Vector_Type is range 0 .. 2**8 - 1;

   type LVT_Entry_Delivery_Type is (Normal);
   for LVT_Entry_Delivery_Type use (Normal => 0);

   type LVT_Entry_Mode_Type is (One_Shot);
   for LVT_Entry_Mode_Type use (One_Shot => 0);

   type LVT_Entry_Mask_Type is (Disable);
   for LVT_Entry_Mask_Type use (Disable => 0);

   type LVT_Entry_Type is record
      Vector   : LVT_Entry_Vector_Type;
      Delivery : LVT_Entry_Delivery_Type;
      Mask     : LVT_Entry_Mask_Type;
      Mode     : LVT_Entry_Mode_Type;
   end record
   with Size => 32, Volatile_Full_Access;

   for LVT_Entry_Type use
   record
      Vector   at 0 range  0 ..  7;
      Delivery at 0 range  8 .. 10;
      Mask     at 0 range 16 .. 16;
      Mode     at 0 range 17 .. 18;
   end record;

   type Timer_Device_Type is record
      Ticks_Per_MS        : Ticks_Type;
      PIT_Calc_Timer_Freq : Ticks_Type;
   end record;

   type Register_32_Type is range 0 .. 2**32 - 1 with Size => 32;
   type Byte_Type is mod 2**8 with Size => 8;

   LVT_Entry : LVT_Entry_Type
   with Address => System'To_Address (LAPIC_LVT_Timer_Entry_Addr);

   Flat_LVT_Entry : Register_32_Type
   with Address => System'To_Address (LAPIC_LVT_Timer_Entry_Addr);

   Divide_Config_Reg : Divide_Configuration_Register_Type
   with Address => System'To_Address (LAPIC_Timer_Divide_Config_Reg_Addr);

   Flat_Divide_Config_Reg : Register_32_Type
   with Address => System'To_Address (LAPIC_Timer_Divide_Config_Reg_Addr);

   Initial_Cnt_Reg : Register_32_Type
   with Address => System'To_Address (LAPIC_Virt_Base + 16#380#);

   Current_Cnt_Reg : Register_32_Type
   with Address => System'To_Address (LAPIC_Virt_Base + 16#390#);

   --
   --  Divide_Config_Reg_Write
   --
   procedure Divide_Config_Reg_Write (Div : Divide_Configuration_Type);

   --
   --  PIT_Measure_Ticks_Per_MS
   --
   procedure PIT_Measure_Ticks_Per_MS (Ticks_Per_MS : out Ticks_Type);

end Timer_Device;
