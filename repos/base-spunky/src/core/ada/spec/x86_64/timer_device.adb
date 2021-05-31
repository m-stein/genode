--
--  \brief  Timer device driver
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

with Interfaces;
with System.Machine_Code;
with Port_IO;

package body Timer_Device
is
   IRQ_ID : constant := 32;

   --
   --  Initialize
   --
   procedure Initialize (Device : out Timer_Device_Type)
   is
      Initial_LVT_Entry : LVT_Entry_Type;
   begin
      --
      --  Enable LAPIC timer in one-shot mode
      --
      Initial_LVT_Entry.Vector   := IRQ_ID;
      Initial_LVT_Entry.Delivery := Normal;
      Initial_LVT_Entry.Mask     := Disable;
      Initial_LVT_Entry.Mode     := One_Shot;
      LVT_Entry := Initial_LVT_Entry;

      Device.Ticks_Per_MS := 0;

      --
      --  Calibrate LAPIC frequency to fullfill our requirements
      --
      Calibration_Loop :
      for Div in reverse Divide_Configuration_Type'Range loop

         exit Calibration_Loop when Device.Ticks_Per_MS >= Min_Ticks_Per_MS;
         Divide_Config_Reg_Write (Div);

         --
         --  Calculate timer frequency
         --
         PIT_Measure_Ticks_Per_MS (Device.Ticks_Per_MS);

      end loop Calibration_Loop;

      if Device.Ticks_Per_MS < Min_Ticks_Per_MS then
         raise Program_Error;
      end if;

      --
      --  Disable PIT timer channel. This is necessary since BIOS sets up
      --  channel 0 to fire periodically.
      --
      Port_IO.Out_Byte (PIT_Mode, 16#30#);
      Port_IO.Out_Byte (PIT_Channel_0_Data, 0);
      Port_IO.Out_Byte (PIT_Channel_0_Data, 0);

   end Initialize;

   --
   --  Divide_Config_Reg_Write
   --
   procedure Divide_Config_Reg_Write (Div : Divide_Configuration_Type)
   is
      New_Divide_Config : Divide_Configuration_Register_Type;
   begin
      New_Divide_Config.Bits_0_To_1 := Natural (Div) mod 2**2;
      New_Divide_Config.Bits_2_To_2 :=
         Natural (Interfaces.Shift_Right (Interfaces.Unsigned_8 (Div), 2))
         mod 2**1;

      Divide_Config_Reg := New_Divide_Config;

   end Divide_Config_Reg_Write;

   --
   --  PIT_Measure_Ticks_Per_MS
   --
   procedure PIT_Measure_Ticks_Per_MS (Ticks_Per_MS : out Ticks_Type)
   is
      use Port_IO;
      Start_Cnt : Register_32_Type;
      End_Cnt : Register_32_Type;
   begin
      --
      --  Set channel gate high and disable speaker
      --
      Port_IO.Out_Byte (
         PIT_Channel_2_Gate,
         ((Port_IO.In_Byte (PIT_Channel_2_Gate) and not
           Port_IO.Byte_Type (2)) or
          Port_IO.Byte_Type (1)));

      --
      --  Set timer counter (mode 0, binary count)
      --
      Port_IO.Out_Byte (PIT_Mode, 16#b0#);
      Port_IO.Out_Byte (
         PIT_Channel_2_Data,
         Port_IO.Byte_Type (PIT_Sleep_Ticks mod 2**8));

      Port_IO.Out_Byte (
         PIT_Channel_2_Data,
         Port_IO.Byte_Type (
            Interfaces.Shift_Right (
               Interfaces.Unsigned_64 (PIT_Sleep_Ticks), 8)));

      Initial_Cnt_Reg := 16#ffff_ffff#;
      Start_Cnt := Current_Cnt_Reg;

      while (Port_IO.In_Byte (PIT_Channel_2_Gate) and 16#20#) = 0 loop

         System.Machine_Code.Asm (
            "pause",
            Clobber => "memory",
            Volatile => True);

      end loop;

      End_Cnt := Current_Cnt_Reg;

      Initial_Cnt_Reg := 0;
      Ticks_Per_MS := Ticks_Type ((Start_Cnt - End_Cnt) / PIT_Sleep_MS);

   end PIT_Measure_Ticks_Per_MS;

   --
   --  Ticks_To_US
   --
   function Ticks_To_US (
      Device : Timer_Device_Type;
      Ticks  : Ticks_Type)
   return Time_Type
   is
      use Interfaces;
      MSB_Mask        : constant := 16#ffff_ffff_0000_0000#;
      LSB_Mask        : constant := 16#0000_0000_ffff_ffff#;
      MSB_Right_Shift : constant := 10;
      LSB_Left_Shift  : constant := 22;

      MSB : constant Unsigned_64 :=
         Shift_Left (
            (
               Shift_Right (
                  Unsigned_64 (Ticks) and MSB_Mask,
                  MSB_Right_Shift
               ) * 1000
            ) / Unsigned_64 (Device.Ticks_Per_MS),
            MSB_Right_Shift
         );

      LSB : constant Unsigned_64 :=
         Shift_Right (
            (
               Shift_Left (
                  Unsigned_64 (Ticks) and LSB_Mask,
                  LSB_Left_Shift
               ) * 1000
            ) / Unsigned_64 (Device.Ticks_Per_MS),
            LSB_Left_Shift
         );
   begin

      return Time_Type (MSB + LSB);

   end Ticks_To_US;

   --
   --  Duration
   --
   function Duration (
      Device                : Timer_Device_Type;
      Last_Timeout_Duration : Time_Type)
   return Time_Type
   is (Last_Timeout_Duration - Time_Type (Current_Cnt_Reg));

   --
   --  Start_One_Shot
   --
   procedure Start_One_Shot (Ticks : Time_Type)
   is
   begin
      Initial_Cnt_Reg := Register_32_Type (Ticks);
   end Start_One_Shot;

   --
   --  Interrupt_ID
   --
   function Interrupt_ID
   return IRQ_ID_Type
   is (IRQ_ID);

   --
   --  US_To_Ticks
   --
   function US_To_Ticks (
      Device : Timer_Device_Type;
      US     : Time_Type)
   return Time_Type
   is ((US / 1000) * Time_Type (Device.Ticks_Per_MS));

end Timer_Device;
