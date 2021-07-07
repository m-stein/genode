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

with CPU_Control_Register_Pkg;
with CPU_Model_Specific_Register_Pkg;

with Interfaces;       use Interfaces;
with CPP;              use CPP;
with CPP_Architecture; use CPP_Architecture;

package CPU_Device_Pkg is

   type CPU_Device_Type  is private;
   type CPU_State_Type   is private;
   type MMU_Context_Type is private;
   type GDT_Type         is private;

   --
   --  Invalidate all entries in translation lookaside buffer
   --
   procedure Invalidate_TLB;

   --
   --  Zero-out RAM region in preparation for being mapped to an address space
   --
   procedure Clear_Memory_Region (
      Address : Address_Type;
      Size    : Size_Type);

   --
   --  Read out the page fault parameters from a given CPU state
   --
   procedure Determine_Page_Fault_State (
      CPU_State :     CPU_State_Type;
      PF_State  : out Page_Fault_State_Type);

   --
   --  Initialize a CPU state object
   --
   procedure Initialize_CPU_State (
      CPU_State       : out CPU_State_Type;
      For_Core_Thread :     Boolean);

   --
   --  Initialize an MMU context object
   --
   procedure Initialize_MMU_Context (
      MMU_Context     : out MMU_Context_Type;
      Page_Table_Addr :     Address_Type);

   --
   --  Determine the kernel name of the CPU that is executing the function
   --
   function ID_Of_Executing_CPU
   return CPU_ID_Type;

   --
   --  Prepare switching execution to a given userland CPU context
   --
   procedure Switch_To (
      CPU_Device  : in out CPU_Device_Type;
      CPU_State   : in out CPU_State_Type;
      MMU_Context :        MMU_Context_Type);

   --
   --  Initialize a CPU Device object
   --
   procedure Start_Initializing_CPU_Device (
      CPU_Device : out CPU_Device_Type);

   --
   --  Initialize the CPU according to an initialized CPU Device object
   --
   procedure Finish_Initializing_CPU_Device (
      CPU_Device : CPU_Device_Type);

   --
   --  Atomically compare and exchange an unsigned 32-bit integer in memory
   --
   --  This function compares the value of 'Value' with 'Compare_Value'.
   --  If both values are equal, 'Value' is set to 'Exchange_Value' and
   --  'Value_Exchanged' is set to 'True'. If the values are different,
   --  'Value' remains unchanged and 'Value_Exchanged' is set to 'False'.
   --  Comparing and exchanging 'Value' is done atomically, i.e. other
   --  CPUs can not interfere with it. This procedure is also a memory
   --  barrier.
   --
   procedure Atomic_Compare_And_Exchange_U32 (
      Value_Exchanged :    out Boolean;
      Value           : access Unsigned_32;
      Compare_Value   :        Unsigned_32;
      Exchange_Value  :        Unsigned_32);

   --
   --  Ensure that all preceding memory access becomes effective
   --
   procedure Memory_Barrier;

private

   IOAPIC_Phys_Addr            : constant := 16#fec00000#;
   IA32_PAT_PA_Write_Combining : constant := 2#001#;

   IDT : Integer
   with
      Import,
      Convention    => C,
      External_Name => "__idt";

   IDT_End : Integer
   with
      Import,
      Convention    => C,
      External_Name => "__idt_end";

   Kernel_Stacks_Area : Integer
   with
      Import,
      Convention    => C,
      External_Name => "kernel_stack";

   Kernel_Stack_Size : Size_Type
   with
      Import,
      Convention    => C,
      External_Name => "kernel_stack_size";

   --
   --  Interrupt Vector Numbers
   --
   --  Intel 64 and IA-32 Architectures Software Developer’s Manual Volume 3A
   --  6.15 EXCEPTION AND INTERRUPT REFERENCE
   --
   type Interrupt_Type is (
      Invalid_Opcode,
      Device_Not_Available,
      Page_Fault,
      First_User_Defined_IRQ,
      Supervisor_Call,
      Reset,
      Last_User_Defined_IRQ);

   for Interrupt_Type use (
      Invalid_Opcode         =>   6,
      Device_Not_Available   =>   7,
      Page_Fault             =>  14,
      First_User_Defined_IRQ =>  32,
      Supervisor_Call        => 128,
      Reset                  => 254,
      Last_User_Defined_IRQ  => 255);

   --
   --  FXSAVE area provides storage for x87 FPU, MMX, XMM, and MXCSR registers.
   --  For further details see Intel SDM Vol. 2A, 'FXSAVE instruction'.
   --
   type FPU_FXSave_Area_Type is array (0 .. 526) of Unsigned_8 with Pack;

   type FPU_State_Type is record
      FCW   : Unsigned_16;
      MXCSR : Unsigned_32;
   end record;

   for FPU_State_Type use record
      FCW   at  0 range 0 .. 15;
      MXCSR at 24 range 0 .. 31;
   end record;

   --
   --  Error code for a page fault
   --
   --  Intel 64 and IA-32 Architectures Software Developer’s Manual Volume 3A
   --  6.15 EXCEPTION AND INTERRUPT REFERENCE
   --  Interrupt 14—Page-Fault Exception (#PF)
   --
   type Page_Fault_Error_Code_Type is record
      P : Bitfield_1_Type;  --  Page present
      W : Bitfield_1_Type;  --  Write access
      I : Bitfield_1_Type;  --  Instruction fetch
   end record
   with Size => 64;

   for Page_Fault_Error_Code_Type use record
      P at 0 range 0 .. 0;
      W at 0 range 1 .. 1;
      I at 0 range 4 .. 4;
   end record;

   --
   --  EFlags register
   --
   --  Intel 64 and IA-32 Architectures Software Developer’s Manual Volume 3A
   --  2.3 SYSTEM FLAGS AND FIELDS IN THE EFLAGS REGISTER
   --
   type EFlags_Type is record
      Interrupt_Enable : Bitfield_1_Type;  --  IF
   end record
   with Size => 64;

   for EFlags_Type use record
      Interrupt_Enable at 0 range 9 .. 9;
   end record;

   type CPU_State_Base_Type is record
      R8            : Unsigned_64;
      R9            : Unsigned_64;
      R10           : Unsigned_64;
      R11           : Unsigned_64;
      R12           : Unsigned_64;
      R13           : Unsigned_64;
      R14           : Unsigned_64;
      R15           : Unsigned_64;
      RAX           : Unsigned_64;
      RBX           : Unsigned_64;
      RCX           : Unsigned_64;
      RDX           : Unsigned_64;
      RDI           : Unsigned_64;
      RSI           : Unsigned_64;
      RBP           : Unsigned_64;
      TrapNo        : Unsigned_64;
      PF_Error_Code : Unsigned_64;  --  Page fault error code
      IP            : Unsigned_64;  --  Instruction pointer
      CS            : Unsigned_64;
      EFlags        : Unsigned_64;  --  EFlags register
      SP            : Unsigned_64;
      SS            : Unsigned_64;
   end record
   with Alignment => 16, Pack;

   type CPU_State_Type is record
      Base          : CPU_State_Base_Type;
      Kernel_Stack  : Unsigned_64;
      FXSave_Addr   : Unsigned_64;
      FXSave_Area   : FPU_FXSave_Area_Type;
   end record
   with Alignment => 16, Pack;

   --
   --  Control register 0
   --
   type CR0_Reg_Type is record
      PE : Bitfield_1_Type;  --  Protection Enable
      MP : Bitfield_1_Type;  --  Monitor Coprocessor
      Em : Bitfield_1_Type;  --  Emulation
      TS : Bitfield_1_Type;  --  Task Switched
      ET : Bitfield_1_Type;  --  Extension Type
      NE : Bitfield_1_Type;  --  Numeric Error
      WP : Bitfield_1_Type;  --  Write Protect
      AM : Bitfield_1_Type;  --  Alignment Mask
      NW : Bitfield_1_Type;  --  Not Write-through
      CD : Bitfield_1_Type;  --  Cache Disable
      Pg : Bitfield_1_Type;  --  Paging
   end record
   with Size => 64;

   for CR0_Reg_Type use record
      PE at 0 range  0 ..  0;
      MP at 0 range  1 ..  1;
      Em at 0 range  2 ..  2;
      TS at 0 range  3 ..  3;
      ET at 0 range  4 ..  4;
      NE at 0 range  5 ..  5;
      WP at 0 range 16 .. 16;
      AM at 0 range 18 .. 18;
      NW at 0 range 29 .. 29;
      CD at 0 range 30 .. 30;
      Pg at 0 range 31 .. 31;
   end record;

   package CR0_Reg is new CPU_Control_Register_Pkg ("0", CR0_Reg_Type);

   --
   --  Control register 2, page-fault linear address (PFLA)
   --
   type CR2_Reg_Type is mod 2**64 with Size => 64;

   package CR2_Reg is new CPU_Control_Register_Pkg ("2", CR2_Reg_Type);

   --
   --  Control register 3
   --
   type CR3_Reg_Type is record
      PWT : Bitfield_1_Type;   --  Page-level write-through
      PCD : Bitfield_1_Type;   --  Page-level cache disable
      PDB : Bitfield_52_Type;  --  Page-directory base address
   end record
   with Size => 64;

   for CR3_Reg_Type use record
      PWT at 0 range  3 ..  3;
      PCD at 0 range  4 ..  4;
      PDB at 0 range 12 .. 63;
   end record;

   package CR3_Reg is new CPU_Control_Register_Pkg ("3", CR3_Reg_Type);

   --
   --  Control register 4
   --
   type CR4_Reg_Type is record
      VME        : Bitfield_1_Type; --  Virtual-8086 Mode Extensions
      PVI        : Bitfield_1_Type; --  Protected-Mode Virtual IRQs
      TSD        : Bitfield_1_Type; --  Time Stamp Disable
      DE         : Bitfield_1_Type; --  Debugging Exceptions
      PSE        : Bitfield_1_Type; --  Page Size Extensions
      PAE        : Bitfield_1_Type; --  Physical Address Extension
      MCE        : Bitfield_1_Type; --  Machine-Check Enable
      PGE        : Bitfield_1_Type; --  Page Global Enable
      PCE        : Bitfield_1_Type; --  Performance-Monitoring Counter Enable
      OSFXSR     : Bitfield_1_Type; --  OS Support for FXSAVE and FXRSTOR instr
      OSXMMExcpt : Bitfield_1_Type; --  OS Support for Unmasked SIMD/FPU Except
      VMXE       : Bitfield_1_Type; --  VMX Enable
      SMXE       : Bitfield_1_Type; --  SMX Enable
      FSGSBase   : Bitfield_1_Type; --  FSGSBASE-Enable
      PCIDE      : Bitfield_1_Type; --  PCIDE Enable
      OSXSave    : Bitfield_1_Type; --  XSAVE and Processor Ext. States-Enable
      SMEP       : Bitfield_1_Type; --  SMEP Enable
      SMAP       : Bitfield_1_Type; --  SMAP Enable
   end record
   with Size => 64;

   for CR4_Reg_Type use record
      VME        at 0 range  0 ..  0;
      PVI        at 0 range  1 ..  1;
      TSD        at 0 range  2 ..  2;
      DE         at 0 range  3 ..  3;
      PSE        at 0 range  4 ..  4;
      PAE        at 0 range  5 ..  5;
      MCE        at 0 range  6 ..  6;
      PGE        at 0 range  7 ..  7;
      PCE        at 0 range  8 ..  8;
      OSFXSR     at 0 range  9 ..  9;
      OSXMMExcpt at 0 range 10 .. 10;
      VMXE       at 0 range 13 .. 13;
      SMXE       at 0 range 14 .. 14;
      FSGSBase   at 0 range 16 .. 16;
      PCIDE      at 0 range 17 .. 17;
      OSXSave    at 0 range 18 .. 18;
      SMEP       at 0 range 20 .. 20;
      SMAP       at 0 range 21 .. 21;
   end record;

   package CR4_Reg is new CPU_Control_Register_Pkg ("4", CR4_Reg_Type);

   --
   --  IA32 APIC base register
   --
   type IA32_APIC_Base_Reg_Type is record
      BSP   : Bitfield_1_Type;   --  Bootstrap processor
      LAPIC : Bitfield_1_Type;   --  Enable/disable local APIC
      Base  : Bitfield_24_Type;  --  Base address of APIC registers
   end record
   with Size => 64;

   for IA32_APIC_Base_Reg_Type use record
      BSP   at 0 range  8 ..  8;
      LAPIC at 0 range 11 .. 11;
      Base  at 0 range 12 .. 35;
   end record;

   package IA32_APIC_Base_Reg is new
      CPU_Model_Specific_Register_Pkg (16#1b#, IA32_APIC_Base_Reg_Type);

   --
   --  IA32 page attribute table register
   --
   type IA32_PAT_Reg_Type is record
      PA1 : Bitfield_3_Type;  --  Page attribute 1
   end record
   with Size => 64;

   for IA32_PAT_Reg_Type use record
      PA1 at 0 range 8 .. 10;
   end record;

   package IA32_PAT_Reg is new
      CPU_Model_Specific_Register_Pkg (16#277#, IA32_PAT_Reg_Type);

   --
   --  Resulting EDX register on CPUID instruction with EAX=1
   --
   type CPUID_1_EDX_Reg_Type is record
      PAT : Bitfield_1_Type;  --  Page attribute table
   end record
   with Size => 64;

   for CPUID_1_EDX_Reg_Type use record
      PAT at 0 range 16 .. 16;
   end record;

   --
   --  Task State Segment (TSS)
   --
   --  See Intel SDM Vol. 3A, section 7.7
   --
   type RSPs_Type is array (0 .. 2) of Unsigned_64 with Pack;
   type ISTs_Type is array (0 .. 6) of Unsigned_64 with Pack;

   type TSS_Type is record
      Reserved_0 : Unsigned_32;  --  Set to 0
      RSPs       : RSPs_Type;    --  PL0-3 stack pointers
      Reserved_1 : Unsigned_64;  --  Set to 0
      ISTs       : ISTs_Type;    --  IRQ stack pointers
      Reserved_2 : Unsigned_64;  --  Set to 0
   end record
   with Alignment => 8, Pack;

   --
   --  Pseudo-descriptor format for registers like GDTR and IDTR
   --
   --  Intel 64 and IA-32 Architectures Software Developer’s Manual Volume 3A
   --  3.5.1 Segment Descriptor Tables
   --
   type Pseudo_Descriptor_Type is record
      Limit : Unsigned_16;
      Base  : Unsigned_64;
   end record
   with Pack;

   --
   --  Global Descriptor Table (GDT)
   --
   --  See Intel SDM Vol. 3A, section 3.5.1
   --
   type TSS_Descriptors_Type is array (0 .. 1) of Unsigned_64 with Pack;

   type GDT_Type is record
      Null_Desc          : Unsigned_64;
      Sys_CS_64_Bit_Desc : Unsigned_64;
      Sys_DS_64_Bit_Desc : Unsigned_64;
      Usr_CS_64_Bit_Desc : Unsigned_64;
      Usr_DS_64_Bit_Desc : Unsigned_64;
      TSS_Descriptors    : TSS_Descriptors_Type;
   end record
   with Alignment => 8, Pack;

   --
   --  CPU device type
   --
   type CPU_Device_Type is record
      TSS : TSS_Type;
      GDT : GDT_Type;
   end record;

   type MMU_Context_Type is record
      CR3 : CR3_Reg_Type;
   end record;

   --
   --  Physical address of the MMIO of the Local APIC
   --
   function LAPIC_Phys_Addr
   return Address_Type;

end CPU_Device_Pkg;
