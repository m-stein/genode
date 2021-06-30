--
--  \brief  Glue code between Ada and C++ interface of the CPU device driver
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

with CPP;              use CPP;
with CPP_Architecture; use CPP_Architecture;
with CPU_Device_Pkg;   use CPU_Device_Pkg;

package CPP_CPU_Device_Pkg is

   function CPU_Device_Size (CPU_Device : CPU_Device_Type)
   return Size_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel11object_sizeERKN6Genode3CpuE";

   procedure Invalidate_TLB
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Genode3Cpu14invalidate_tlbEv";

   procedure Clear_Memory_Region (
      Address                  : Address_Type;
      Size                     : Size_Type;
      Changed_Cache_Properties : Bool_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Genode3Cpu19clear_memory_regionEmmb";

   procedure Determine_Page_Fault_State (
      CPU_State :     CPU_State_Type;
      PF_State  : out Page_Fault_State_Type)
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN6Genode3Cpu9mmu_faultERNS0_7ContextERN6Kernel12Thread_faultE";

   procedure Initialize_CPU_State (
      CPU_State       : out CPU_State_Type;
      For_Core_Thread :     Bool_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Genode3Cpu7ContextC2Eb";

   procedure Initialize_MMU_Context (
      MMU_Context     : out MMU_Context_Type;
      Page_Table_Addr :     Address_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Genode3Cpu11Mmu_contextC1Em";

   function ID_Of_Executing_CPU
   return CPU_ID_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Genode3Cpu12executing_idEv";

   procedure Switch_To (
      CPU_Device  : in out CPU_Device_Type;
      CPU_State   : in out CPU_State_Type;
      MMU_Context :        MMU_Context_Type)
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN6Genode3Cpu9switch_toERNS0_7ContextERNS0_11Mmu_contextE";

   procedure Initialize_CPU_Device (
      CPU_Device : out CPU_Device_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Genode3CpuC1Ev";

end CPP_CPU_Device_Pkg;
