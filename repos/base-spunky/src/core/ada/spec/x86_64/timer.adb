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

with Log;
with Interfaces;

package body Timer is

   --
   --  Initialize
   --
   procedure Initialize (Tmr : out Timer_Type) is
   begin
      --
      --  Enable LAPIC timer in one-shot mode
      --
      LVT_Entry.Vector := 32;
      LVT_Entry.Delivery := Normal;
      LVT_Entry.Mask := Disable;
      LVT_Entry.Mode := One_Shot;

      --
      --  Calibrate LAPIC frequency to fullfill our requirements
      --
      Calibration_Loop :
      for Div in reverse Divide_Configuration_Type'Range loop

         exit Calibration_Loop when Tmr.Ticks_Per_MS >= Min_Ticks_Per_MS;
         Divide_Config_Reg_Write (Div);
         PIT_Measure_Ticks_Per_MS (Tmr.Ticks_Per_MS);

      end loop Calibration_Loop;

      if Tmr.Ticks_Per_MS < Min_Ticks_Per_MS then
         Log.Print_String ("Failed to calibrate timer frequency");
         raise Program_Error;
      end if;

      --
      --  Disable PIT timer channel. This is necessary since BIOS sets up
      --  channel 0 to fire periodically.
      --
      Port_IO_Out_Byte (PIT_Mode, 16#30#);
      Port_IO_Out_Byte (PIT_Channel_0_Data, 0);
      Port_IO_Out_Byte (PIT_Channel_0_Data, 0);

   end Initialize;

   --
   --  Divide_Config_Reg_Write
   --
   procedure Divide_Config_Reg_Write (Div : Divide_Configuration_Type)
   is
   begin
      Divide_Config_Reg.Bits_0_To_1 := Natural (Div) and 3;
      Divide_Config_Reg.Bits_2_To_2 := Interfaces.Shift_Right (Div, 2) and 1;

   end Divide_Config_Reg_Write;

   --
   --  Port_IO_Out_Byte
   --
   procedure Port_IO_Out_Byte (
      Port  : Port_IO_Port_Type;
      Value : Byte_Type)
   is
   begin
   end Port_IO_Out_Byte;

   --
   --  Port_IO_In_Byte
   --
   function Port_IO_In_Byte (Port : Port_IO_Port_Type)
   return Byte_Type
   is
      Value : Byte_Type;
   begin
   end Port_IO_In_Byte;

   --
   --  PIT_Measure_Ticks_Per_MS
   --
   procedure PIT_Measure_Ticks_Per_MS (Ticks_Per_MS : out Ticks_Type)
   is
      Start_Cnt : Ticks_Type;
      End_Cnt : Ticks_Type;
   begin
      --
      --  Set channel gate high and disable speaker
      --
      Port_IO_Out_Byte (
         PIT_Channel_2_Gate,
         ((Port_IO_In_Byte (PIT_Ch2_Gate) and not Byte_Type (2)) or
          Byte_Type (1)));

      --
      --  Set timer counter (mode 0, binary count)
      --
      Port_IO_Out_Byte (PIT_Mode, 16#b0#);
      Port_IO_Out_Byte (
         PIT_Channel_2_Data,
         Byte_Type (PIT_Sleep_Ticks and 16#ff#));

      Port_IO_Out_Byte (
         PIT_Channel_2_Data,
         Byte_Type (System.Shift_Right (PIT_Sleep_Ticks, 8)));

      Initial_Cnt_Reg := 16#ffffffff#;
      Start_Cnt := Current_Cnt_Reg;
      while (Port_IO_In_Byte (PIT_Channel_2_Gate) and 16#20#) = 0 loop
         Asm ("pause", Clobber => "memory");
      end loop;
      End_Cnt := Current_Cnt_Reg;
      Initial_Cnt_Reg := 0;
      Ticks_Per_MS := (Start_Cnt - End_Cnt) / PIT_Sleep_MS;

   end PIT_Measure_Ticks_Per_MS;

end Timer;
