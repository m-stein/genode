--
--  \brief  CPU device driver
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

with System.Storage_Elements;

with System.Machine_Code; use System.Machine_Code;

package body CPU_Device_Pkg is

   procedure Switch_To (
      CPU_Device  : in out CPU_Device_Type;
      CPU_State   : in out CPU_State_Type;
      MMU_Context :        MMU_Context_Type)
   is
      Stacks_Area_Addr : constant Address_Type :=
         Addr_To_U64 (Kernel_Stacks_Area'Address);
   begin

      if CPU_State.Base.CS /= 8 and then
         MMU_Context.CR3 /= CR3_Reg.Read
      then
         CR3_Reg.Write (MMU_Context.CR3);
      end if;

      CPU_Device.TSS.ISTs (0) :=
         Addr_To_U64 (CPU_State'Address) + CPU_State_Base_Type'Size / 8;

      CPU_State.Kernel_Stack :=
         Stacks_Area_Addr +
         (Address_Type (ID_Of_Executing_CPU) + 1) * Kernel_Stack_Size -
         Address_Type'Size / 8;

   end Switch_To;

   function ID_Of_Executing_CPU
   return CPU_ID_Type
   is
      Stack_Var : constant Integer := 0;
      Stack_Var_Addr : constant Address_Type :=
         Addr_To_U64 (Stack_Var'Address);

      Stacks_Area_Addr : constant Address_Type :=
         Addr_To_U64 (Kernel_Stacks_Area'Address);

   begin
      return
         CPU_ID_Type ((Stack_Var_Addr - Stacks_Area_Addr) / Kernel_Stack_Size);

   end ID_Of_Executing_CPU;

   procedure Initialize_MMU_Context (
      MMU_Context     : out MMU_Context_Type;
      Page_Table_Addr :     Address_Type)
   is
      CR3_U64 : Unsigned_64 with Address => MMU_Context.CR3'Address;
   begin
      CR3_U64 := 0;
      MMU_Context.CR3.PDB :=
        Bitfield_52_Type (Shift_Right (Page_Table_Addr, 12));

   end Initialize_MMU_Context;

   function LAPIC_Phys_Addr
   return Address_Type
   is (
      Address_Type (
         Shift_Left (Unsigned_64 (IA32_APIC_Base_Reg.Read.Base), 12)));

   procedure Invalidate_TLB
   is
   begin
      declare
         CR3 : constant CR3_Reg_Type := CR3_Reg.Read;
      begin
         CR3_Reg.Write (CR3);
      end;
   end Invalidate_TLB;

   procedure Initialize_CPU_State (
      CPU_State       : out CPU_State_Type;
      For_Core_Thread :     Boolean)
   is
   begin
      CPU_State.Base.R8            := 0;
      CPU_State.Base.R9            := 0;
      CPU_State.Base.R10           := 0;
      CPU_State.Base.R11           := 0;
      CPU_State.Base.R12           := 0;
      CPU_State.Base.R13           := 0;
      CPU_State.Base.R14           := 0;
      CPU_State.Base.R15           := 0;
      CPU_State.Base.RAX           := 0;
      CPU_State.Base.RBX           := 0;
      CPU_State.Base.RCX           := 0;
      CPU_State.Base.RDX           := 0;
      CPU_State.Base.RDI           := 0;
      CPU_State.Base.RSI           := 0;
      CPU_State.Base.RBP           := 0;
      CPU_State.Base.TrapNo        := Reset'Enum_Rep;
      CPU_State.Base.PF_Error_Code := 0;
      CPU_State.Base.IP            := 0;
      CPU_State.Base.EFlags        := 0;
      CPU_State.Base.SP            := 0;

      if For_Core_Thread then
         CPU_State.Base.CS := 16#8#;
         CPU_State.Base.SS := 16#10#;
      else
         CPU_State.Base.CS := 16#1b#;
         CPU_State.Base.SS := 16#23#;
      end if;

      Declare_EFlags :
      declare
         EFlags : EFlags_Type with Address => CPU_State.Base.EFlags'Address;
      begin
         EFlags.Interrupt_Enable := 1;
      end Declare_EFlags;

      Declare_FPU_State :
      declare
         FPU_State_Addr : constant System.Address :=
            Round_Up_Address (CPU_State.FXSave_Area'Address, 4);

         FPU_State : FPU_State_Type with Address => FPU_State_Addr;
      begin
         CPU_State.FXSave_Area := (others => 0);
         CPU_State.FXSave_Addr :=
            Unsigned_64 (System.Storage_Elements.To_Integer (FPU_State_Addr));

         --
         --  Mask exceptions SysV ABI
         --
         FPU_State.FCW   := 16#37f#;
         FPU_State.MXCSR := 16#1f80#;
      end Declare_FPU_State;

   end Initialize_CPU_State;

   procedure Clear_Memory_Region (
      Address : Address_Type;
      Size    : Size_Type)
   is
   begin
      if Round_Up_U64 (Address, 3) = Address and then
         Round_Up_U64 (Size, 3) = Size
      then
         Declare_Asm_Outputs :
         declare
            Asm_Start_Output : Unsigned_64 := Address;
            Asm_Count_Output : Unsigned_64 := Size / 8;
         begin

            Asm (
               "rep stosq",
               Inputs =>
                  Unsigned_64'Asm_Input ("a", 0),
               Outputs => (
                  Unsigned_64'Asm_Output ("+D", Asm_Start_Output),
                  Unsigned_64'Asm_Output ("+c", Asm_Count_Output)),
               Clobber  => "memory",
               Volatile => True);

         end Declare_Asm_Outputs;
      else
         Declare_Memory_Region :
         declare
            type Memory_Region_Type is array (0 .. Size - 1) of Unsigned_8;
            Memory_Region :
               Memory_Region_Type with Address => U64_To_Addr (Address);
         begin

            Memory_Region := (others => 0);

         end Declare_Memory_Region;
      end if;

   end Clear_Memory_Region;

   procedure Initialize_CPU_Device (
      CPU_Device : out CPU_Device_Type)
   is
      TSS_Addr : constant Address_Type := Addr_To_U64 (CPU_Device.TSS'Address);

      TSSD_Bits_1 : constant Unsigned_64 :=
         Shift_Left (Shift_Right (TSS_Addr, 24) and 16#ff#, 24);

      TSSD_Bits_2 : constant Unsigned_64 :=
         Shift_Right (TSS_Addr, 16) and 16#ff#;

      TSSD_Bits_3 : constant Unsigned_64 :=
         Shift_Left (TSS_Addr and 16#ffff#, 16) or 16#68#;

      TSSD_Bits_4 : constant Unsigned_64 := Shift_Right (TSS_Addr, 32);

      GDT_Descr : Pseudo_Descriptor_Type;
      IDT_Descr : Pseudo_Descriptor_Type;

      TSS_Selector : constant := 16#28#;
   begin

      --
      --  Initialize task-state segment
      --
      CPU_Device.TSS.Reserved_0 := 0;
      CPU_Device.TSS.RSPs       := (others => 0);
      CPU_Device.TSS.Reserved_1 := 0;
      CPU_Device.TSS.ISTs       := (others => 0);
      CPU_Device.TSS.Reserved_2 := 0;

      --
      --  Initialize global descriptor table
      --
      CPU_Device.GDT.Null_Desc          := 0;
      CPU_Device.GDT.Sys_CS_64_Bit_Desc := 16#20_9800_0000_0000#;
      CPU_Device.GDT.Sys_DS_64_Bit_Desc := 16#20_9300_0000_0000#;
      CPU_Device.GDT.Usr_CS_64_Bit_Desc := 16#20_f800_0000_0000#;
      CPU_Device.GDT.Usr_DS_64_Bit_Desc := 16#20_f300_0000_0000#;

      CPU_Device.GDT.TSS_Descriptors (0) :=
         Shift_Left (TSSD_Bits_1 or TSSD_Bits_2 or 16#8900#, 32) or
         TSSD_Bits_3;

      CPU_Device.GDT.TSS_Descriptors (1) := TSSD_Bits_4;

      --
      --  Initialize global descriptor-table register
      --
      GDT_Descr.Limit := GDT_Type'Size / 8;
      GDT_Descr.Base := Addr_To_U64 (CPU_Device.GDT'Address);
      Asm (
         "lgdt (%0)",
         Inputs =>
            Unsigned_64'Asm_Input ("r", Addr_To_U64 (GDT_Descr'Address)),
         Volatile => True);

      --
      --  Initialize interrupt-descriptor-table register
      --
      IDT_Descr.Limit :=
         Unsigned_16 (
            Addr_To_U64 (IDT_End'Address) - Addr_To_U64 (IDT'Address));

      IDT_Descr.Base := Addr_To_U64 (IDT'Address);
      Asm (
         "lidt (%0)",
         Inputs =>
            Unsigned_64'Asm_Input ("r", Addr_To_U64 (IDT_Descr'Address)),
         Volatile => True);

      --
      --  Initialize task register
      --
      Asm (
         "ltr %w0",
         Inputs   => Unsigned_64'Asm_Input ("r", TSS_Selector),
         Volatile => True);

   end Initialize_CPU_Device;

   procedure Determine_Page_Fault_State (
      CPU_State :     CPU_State_Type;
      PF_State  : out Page_Fault_State_Type)
   is
      PF_Error_Code : Page_Fault_Error_Code_Type with
         Address => CPU_State.Base.PF_Error_Code'Address;
   begin
      PF_State.IP      := 0;
      PF_State.Address := Address_Type (CR2_Reg.Read);
      if PF_Error_Code.W = 1 then

         PF_State.Reason := Write;

      elsif PF_Error_Code.P = 0 then

         PF_State.Reason := Page_Missing;

      elsif PF_Error_Code.I = 1 then

         PF_State.Reason := Execute;

      else
         PF_State.Reason := Unknown;

      end if;
   end Determine_Page_Fault_State;

end CPU_Device_Pkg;
