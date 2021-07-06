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

with Port_IO;

with System.Machine_Code; use System.Machine_Code;
with Log;                 use Log;

package body IRQ_Controller_Pkg is

   procedure Initialize_IRQ_Controller_Pkg
   is
   begin
      Stored_APIC_IDs := (others => 0);
      IRQ_Modes       := (
         others => (
            Trigger_Mode => Invalid,
            Polarity     => Invalid));

   end Initialize_IRQ_Controller_Pkg;

   --
   --  Initialize
   --
   procedure Initialize (Ctrl : IRQ_Controller_Reference_Type)
   is
   begin

      --
      --  I/O APIC
      --

      --  Read the number of redirection entries
      IOAPIC_IORegSel_Reg := IOAPIC_IORegSel_IOAPICVer;
      Ctrl.Number_Of_IORedTbl_Regs :=
         Number_Of_IORedTbl_Regs_Type (
            IOAPIC_IOAPICVer_Reg.Max_Redirection_Entry + 1);

      For_Each_IRQ :
      for Idx in IRQ_Index_Type'Range loop

         --
         --  Set legacy/ISA IRQs to edge-triggerd and high polarity
         --
         if Idx in ISA_IRQ_Index_Type then
            IRQ_Modes (Idx).Trigger_Mode := Edge;
            IRQ_Modes (Idx).Polarity     := High;
         else
            IRQ_Modes (Idx).Trigger_Mode := Level;
            IRQ_Modes (Idx).Polarity     := Low;
         end if;

         --
         --  Remap all IRQs managed by the I/O APIC
         --
         if Idx < IRQ_Index_Type (Ctrl.Number_Of_IORedTbl_Regs) then

            Declare_IORedTbl :
            declare
               IORedTbl_Low  : IOAPIC_IORedTbl_Low_Reg_Type;
               IORedTbl_High : IOAPIC_IORedTbl_High_Reg_Type;
            begin

               IORedTbl_Low.Vector := Bitfield_8_Type (IRQ_Remap_Base + Idx);
               IORedTbl_Low.Mask := 1;
               IORedTbl_Low.Delivery_Mode := 0;
               IORedTbl_Low.Destination_Mode := 0;
               IORedTbl_Low.Remote_IRR := 0;
               IORedTbl_Low.Delivery_Status := 0;
               IORedTbl_Low.Polarity :=
                  IRQ_Polarity_To_Bitfield_1 (IRQ_Modes (Idx).Polarity);

               IORedTbl_Low.Trigger_Mode :=
                  IRQ_Trigger_Mode_To_Bitfield_1 (
                     IRQ_Modes (Idx).Trigger_Mode);

               IORedTbl_High.Destination_Field := 0;

               IOAPIC_IORegSel_Reg :=
                  Unsigned_32 (IOAPIC_IORegSel_IORedTbl + 2 * Idx + 1);

               IOAPIC_IORedTbl_High_Reg := IORedTbl_High;

               IOAPIC_IORegSel_Reg :=
                  Unsigned_32 (IOAPIC_IORegSel_IORedTbl + 2 * Idx);

               IOAPIC_IORedTbl_Low_Reg := IORedTbl_Low;

            end Declare_IORedTbl;

         end if;

      end loop For_Each_IRQ;

      --
      --  Local APIC
      --

      --  Start initialization sequence in cascade mode
      Port_IO.Out_Byte (PIC_Cmd_Master, 16#11#);
      Port_IO.Out_Byte (PIC_Cmd_Slave, 16#11#);

      --  ICW2: Master PIC vector offset (32)
      Port_IO.Out_Byte (PIC_Data_Master, 16#20#);

      --  ICW2: Slave PIC vector offset (40)
      Port_IO.Out_Byte (PIC_Data_Slave, 16#28#);

      --  ICW3: Tell Master PIC that there is a slave PIC at IRQ2
      Port_IO.Out_Byte (PIC_Data_Master, 4);

      --  ICW3: Tell Slave PIC its cascade identity
      Port_IO.Out_Byte (PIC_Data_Slave, 2);

      --  ICW4: Enable 8086 mode
      Port_IO.Out_Byte (PIC_Data_Master, 16#01#);
      Port_IO.Out_Byte (PIC_Data_Slave,  16#01#);

      --  Disable legacy pic
      Port_IO.Out_Byte (PIC_Data_Slave,  16#ff#);
      Port_IO.Out_Byte (PIC_Data_Master, 16#ff#);

      --  Enable local APIC
      Declare_LAPIC_SVR :
      declare
         LAPIC_SVR : LAPIC_SVR_Reg_Type := LAPIC_SVR_Reg;
      begin

         LAPIC_SVR.APIC_Enable := 1;
         LAPIC_SVR_Reg := LAPIC_SVR;

      end Declare_LAPIC_SVR;

   end Initialize;

   --
   --  Builtin_FFS
   --
   --  Input, output, and side effects checked against base-hw
   --
   function Builtin_FFS (Value : Unsigned_32)
   return Unsigned_32
   is
      Tmp_Value  : Unsigned_32 := Value;
      Nr_Of_Bits : Unsigned_32 := 16;
      Bit_Idx    : Unsigned_32 :=  0;
   begin
      Main_Loop :
      loop
         declare
            Mask : constant Unsigned_32 :=
               Shift_Left (1, Integer (Nr_Of_Bits)) - 1;
         begin
            if (Tmp_Value and Mask) = 0 then
               Tmp_Value  :=
                  Shift_Right (Tmp_Value, Integer (Nr_Of_Bits));
               Bit_Idx := Bit_Idx + Nr_Of_Bits;
            end if;
            if Nr_Of_Bits > 1 then
               Nr_Of_Bits := Shift_Right (Nr_Of_Bits, 1);
            else
               exit Main_Loop;
            end if;
         end;
      end loop Main_Loop;
      if Tmp_Value > 0 then
         return Bit_Idx + 1;
      else
         return 0;
      end if;
   end Builtin_FFS;

   --
   --  LAPIC_Get_Lowest_Bit
   --
   --  Input, output, and side effects checked against base-hw
   --
   function LAPIC_Get_Lowest_Bit
   return Unsigned_32
   is
      Bit      : Unsigned_32 := 0;
      Vec_Base : Unsigned_32 := 0;
   begin
      Bit := Builtin_FFS (LAPIC_ISR_0_Reg);
      if Bit /= 0 then
         return Vec_Base + Bit;
      end if;
      Vec_Base := Vec_Base + 32;

      Bit := Builtin_FFS (LAPIC_ISR_1_Reg);
      if Bit /= 0 then
         return Vec_Base + Bit;
      end if;
      Vec_Base := Vec_Base + 32;

      Bit := Builtin_FFS (LAPIC_ISR_2_Reg);
      if Bit /= 0 then
         return Vec_Base + Bit;
      end if;
      Vec_Base := Vec_Base + 32;

      Bit := Builtin_FFS (LAPIC_ISR_3_Reg);
      if Bit /= 0 then
         return Vec_Base + Bit;
      end if;
      Vec_Base := Vec_Base + 32;

      Bit := Builtin_FFS (LAPIC_ISR_4_Reg);
      if Bit /= 0 then
         return Vec_Base + Bit;
      end if;
      Vec_Base := Vec_Base + 32;

      Bit := Builtin_FFS (LAPIC_ISR_5_Reg);
      if Bit /= 0 then
         return Vec_Base + Bit;
      end if;
      Vec_Base := Vec_Base + 32;

      Bit := Builtin_FFS (LAPIC_ISR_6_Reg);
      if Bit /= 0 then
         return Vec_Base + Bit;
      end if;
      Vec_Base := Vec_Base + 32;

      Bit := Builtin_FFS (LAPIC_ISR_7_Reg);
      if Bit /= 0 then
         return Vec_Base + Bit;
      end if;
      Vec_Base := Vec_Base + 32;

      return 0;

   end LAPIC_Get_Lowest_Bit;

   --
   --  Take_Request
   --
   procedure Take_Request (
      IRQ_ID       : in out IRQ_ID_Type;
      IRQ_ID_Valid : in out Boolean)
   is
   begin
      IRQ_ID := IRQ_ID_Type (LAPIC_Get_Lowest_Bit);
      if IRQ_ID = 0 then
         IRQ_ID_Valid := False;
      else
         IRQ_ID := IRQ_ID - 1;
         IRQ_ID_Valid := True;
      end if;
   end Take_Request;

   --
   --  Finish_Request
   --
   procedure Finish_Request
   is
   begin
      LAPIC_EOI := 0;
   end Finish_Request;

   --
   --  IOAPIC_Toggle_IRQ_Mask
   --
   procedure IOAPIC_Toggle_IRQ_Mask (
      Ctrl   : IRQ_Controller_Reference_Type;
      IRQ_ID : IRQ_ID_Type;
      Mask   : Boolean)
   is
      IRQ_Idx : constant IRQ_Index_Type :=
         IRQ_Index_Type (IRQ_ID - IRQ_Remap_Base);
   begin
      --
      --  Ignore toggle requests for vectors not handled by the I/O APIC
      --
      if IRQ_ID  < IRQ_Remap_Base or else
         IRQ_ID >= IRQ_ID_Type (IRQ_Remap_Base + Ctrl.Number_Of_IORedTbl_Regs)
      then
         return;
      end if;

      --
      --  Only mask existing RTEs and do *not* mask edge-triggered
      --  interrupts to avoid losing them while masked, see Intel
      --  82093AA I/O Advanced Programmable Interrupt Controller
      --  (IOAPIC) specification, section 3.4.2, "Interrupt Mask"
      --  flag and edge-triggered interrupts or:
      --  http://yarchive.net/comp/linux/edge_triggered_interrupts.html
      --
      if IRQ_Modes (IRQ_Idx).Trigger_Mode = Edge and then Mask then
         return;
      end if;

      --
      --  Set mask bit in redirection table entry of the IRQ_Idx
      --
      IOAPIC_IORegSel_Reg :=
         Unsigned_32 (IOAPIC_IORegSel_IORedTbl + (2 * IRQ_Idx));

      Declare_IORedTbl_Low :
      declare
         IORedTbl_Low : IOAPIC_IORedTbl_Low_Reg_Type :=
            IOAPIC_IORedTbl_Low_Reg;
      begin
         if Mask then
            IORedTbl_Low.Mask := 1;
         else
            IORedTbl_Low.Mask := 0;
         end if;
         IOAPIC_IORedTbl_Low_Reg := IORedTbl_Low;
      end Declare_IORedTbl_Low;

   end IOAPIC_Toggle_IRQ_Mask;

   --
   --  Mask
   --
   procedure Mask (
      Ctrl   : IRQ_Controller_Reference_Type;
      IRQ_ID : IRQ_ID_Type)
   is
   begin
      IOAPIC_Toggle_IRQ_Mask (Ctrl, IRQ_ID, True);
   end Mask;

   --
   --  Unmask
   --
   procedure Unmask (
      Ctrl   : IRQ_Controller_Reference_Type;
      IRQ_ID : IRQ_ID_Type)
   is
   begin
      IOAPIC_Toggle_IRQ_Mask (Ctrl, IRQ_ID, False);
   end Unmask;

   --
   --  IRQ_Mode
   --
   procedure IRQ_Mode (
      IRQ_ID       : IRQ_ID_Type;
      Trigger_Mode : IRQ_Trigger_Mode_Type;
      Polarity     : IRQ_Polarity_Type)
   is
      IRQ_Idx : constant IRQ_Index_Type :=
         IRQ_Index_Type (IRQ_ID - IRQ_Remap_Base);
   begin

      IRQ_Modes (IRQ_Idx).Trigger_Mode := Trigger_Mode;
      IRQ_Modes (IRQ_Idx).Polarity := Polarity;

      --
      --  Update redirection table entry of the IRQ
      --
      IOAPIC_IORegSel_Reg :=
         Unsigned_32 (IOAPIC_IORegSel_IORedTbl + (2 * IRQ_Idx));

      Declare_IORedTbl_Low :
      declare
         IORedTbl_Low : IOAPIC_IORedTbl_Low_Reg_Type :=
            IOAPIC_IORedTbl_Low_Reg;
      begin

         IORedTbl_Low.Polarity :=
            IRQ_Polarity_To_Bitfield_1 (IRQ_Modes (IRQ_Idx).Polarity);

         IORedTbl_Low.Trigger_Mode :=
            IRQ_Trigger_Mode_To_Bitfield_1 (IRQ_Modes (IRQ_Idx).Trigger_Mode);

         IOAPIC_IORedTbl_Low_Reg := IORedTbl_Low;

      end Declare_IORedTbl_Low;

   end IRQ_Mode;

   --
   --  Send_IPI
   --
   procedure Send_IPI (
      Ctrl    : IRQ_Controller_Reference_Type;
      CPU_Idx : CPU_Index_Type)
   is
      pragma Unreferenced (Ctrl);
      ICR_High : LAPIC_ICR_High_Reg_Type;
      ICR_Low  : LAPIC_ICR_Low_Reg_Type;
      ICR_High_U32 : Unsigned_32 with Address => ICR_High'Address;
      ICR_Low_U32  : Unsigned_32 with Address => ICR_Low'Address;
   begin

      Wait_For_Delivery_Status :
      while LAPIC_ICR_Low_Reg.Delivery_Status /= 0 loop

         Asm ("pause", Clobber => "memory", Volatile => True);

      end loop Wait_For_Delivery_Status;

      ICR_High.Destination    := Bitfield_8_Type (Stored_APIC_IDs (CPU_Idx));
      ICR_Low.Vector          := Interprocessor_IRQ_ID;
      ICR_Low.Delivery_Status := 0;
      ICR_Low.Level_Assert    := 1;

      LAPIC_ICR_High_Reg := ICR_High;
      LAPIC_ICR_Low_Reg  := ICR_Low;

   end Send_IPI;

   --
   --  Store_APIC_ID
   --
   procedure Store_APIC_ID (
      Ctrl    : IRQ_Controller_Reference_Type;
      CPU_Idx : CPU_Index_Type)
   is
      pragma Unreferenced (Ctrl);
   begin
      Stored_APIC_IDs (CPU_Idx) := Stored_APIC_ID_Type (LAPIC_ID_Reg.APIC_ID);
   end Store_APIC_ID;

   --
   --  Number_Of_IRQs
   --
   function Number_Of_IRQs
   return Number_Of_IRQs_Type
   is (
      Number_Of_IRQs_Type (IRQ_Index_Type'Range_Length));

   --
   --  Interprocessor_IRQ
   --
   function Interprocessor_IRQ
   return IRQ_ID_Type
   is (
      Interprocessor_IRQ_ID);

   --
   --  IRQ_Polarity_To_Bitfield_1
   --
   function IRQ_Polarity_To_Bitfield_1 (Polarity : IRQ_Polarity_Type)
   return Bitfield_1_Type
   is
   begin
      case Polarity is
      when High    => return 0;
      when Low     => return 1;
      when Invalid =>
         Print_String ("Error: Invalid IRQ polarity");
         raise Program_Error;
      end case;
   end IRQ_Polarity_To_Bitfield_1;

   --
   --  IRQ_Trigger_Mode_To_Bitfield_1
   --
   function IRQ_Trigger_Mode_To_Bitfield_1 (Mode : IRQ_Trigger_Mode_Type)
   return Bitfield_1_Type
   is
   begin
      case Mode is
      when Edge    => return 0;
      when Level   => return 1;
      when Invalid =>
         Print_String ("Error: Invalid IRQ trigger mode");
         raise Program_Error;
      end case;
   end IRQ_Trigger_Mode_To_Bitfield_1;

end IRQ_Controller_Pkg;
