--
--  \brief  Timer driver
--  \author Martin stein
--  \date   2020-02-02
--

--
--  Copyright (C) 2019 Genode Labs GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

pragma Ada_2012;

with System;

package Timer is

   type Timer_Type is private;

   --
   --  Initialize
   --
   procedure Initialize (Tmr : out Timer_Type);

private

   LAPIC_LVT_Timer_Entry_Addr : constant := 16#320#;
   LAPIC_Timer_Divide_Config_Reg_Addr : constant := 16#3e0#;

   Min_Ticks_Per_MS : constant := 1000;

   PIT_Channel_2_Gate : constant := 16#61#;
   PIT_Channel_2_Data : constant := 16#42#;
   PIT_Channel_0_Data : constant := 16#40#;
   PIT_Mode : constant := 16#43#;

   PIT_Ticks_Per_Sec  : constant := 1193182;
   PIT_Sleep_MS : constant := 50;
   PIT_Sleep_Ticks : constant := (PIT_Ticks_Per_Sec / 1000) * PIT_Sleep_MS;

   type Divide_Configuration_Type is range 1 .. 6;

   type Divide_Configuration_Register_Type is record
      Bits_0_To_1 : Natural range 0 .. 2**2 - 1;
      Bits_2_To_2 : Natural range 0 .. 2**1 - 1;
   end record;

   for Divide_Configuration_Register_Type use
   record
      Bits_0_To_1 at 0 range 0 .. 1;
      Bits_2_To_2 at 0 range 3 .. 3;
   end record;

   type Ticks_Type is range 0 .. 32**2 - 1;

   type LVT_Entry_Vector_Type is range 0 .. 2**8 - 1;

   type LVT_Entry_Delivery_Type is (Normal);
   for LVT_Entry_Delivery_Type use (Normal => 0);

   type LVT_Entry_Mode_Type is (One_Shot);
   for LVT_Entry_Mode_Type use (One_Shot => 0);

   type LVT_Entry_Mask_Type is (Disable);
   for LVT_Entry_Mask_Type use (Disable => 0);

   type LVT_Entry_Type is record
      Vector : LVT_Entry_Vector_Type;
      Delivery : LVT_Entry_Delivery_Type;
      Mask : LVT_Entry_Mask_Type;
      Mode : LVT_Entry_Mode_Type;
   end record;

   for LVT_Entry_Type use
   record
      Vector at 0 range 0 .. 7;
      Delivery at 0 range 8 .. 10;
      Mask at 0 range 16 .. 16;
      Mode at 0 range 17 .. 18;
   end record;

   type Timer_Type is record
      Ticks_Per_MS        : Ticks_Type;
      PIT_Calc_Timer_Freq : Ticks_Type;
   end record;

   LVT_Entry : LVT_Entry_Type
   with Address => System'To_Address (LAPIC_LVT_Timer_Entry_Addr);

   Divide_Config_Reg : Divide_Configuration_Register_Type
   with Address => System'To_Address (LAPIC_Timer_Divide_Config_Reg_Addr);

   --
   --  Port_IO_Out_Byte
   --
   procedure Port_IO_Out_Byte (
      Port  : Port_IO_Port_Type;
      Value : Byte_Type);

   --
   --  Port_IO_In_Byte
   --
   function Port_IO_In_Byte (Port : Port_IO_Port_Type)
   return Byte_Type;

   --
   --  Divide_Config_Reg_Write
   --
   procedure Divide_Config_Reg_Write (Div : Divide_Configuration_Type);

   --
   --  PIT_Measure_Ticks_Per_MS
   --
   procedure PIT_Measure_Ticks_Per_MS (Ticks_Per_MS : out Ticks_Type);

end Timer;
