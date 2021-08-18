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

with Interfaces; use Interfaces;

package body CPP_CPU_Device_Pkg is

   function CPU_Device_Size (CPU_Device : CPU_Device_Type)
   return Size_Type
   is (CPU_Device'Size / 8);

   function CPU_State_Size (CPU_State : CPU_State_Type)
   return Size_Type
   is (CPU_State'Size / 8);

   function MMU_Context_Size (MMU_Context : MMU_Context_Type)
   return Size_Type
   is (MMU_Context'Size / 8);

   procedure Switch_To (
      CPU_Device  : in out CPU_Device_Type;
      CPU_State   : in out CPU_State_Type;
      MMU_Context :        MMU_Context_Type)
   is
   begin
      CPU_Device_Pkg.Switch_To (CPU_Device, CPU_State, MMU_Context);
   end Switch_To;

   function ID_Of_Executing_CPU
   return CPU_ID_Type
   is (CPU_Device_Pkg.ID_Of_Executing_CPU);

   procedure Invalidate_TLB
   is
   begin
      CPU_Device_Pkg.Invalidate_TLB;
   end Invalidate_TLB;

   procedure Clear_Memory_Region (
      Address                  : Address_Type;
      Size                     : Size_Type;
      Changed_Cache_Properties : Bool_Type)
   is
      pragma Unreferenced (Changed_Cache_Properties);
   begin
      CPU_Device_Pkg.Clear_Memory_Region (Address, Size);
   end Clear_Memory_Region;

   procedure Determine_Page_Fault_State (
      CPU_State :     CPU_State_Type;
      PF_State  : out Page_Fault_State_Type)
   is
   begin
      CPU_Device_Pkg.Determine_Page_Fault_State (CPU_State, PF_State);
   end Determine_Page_Fault_State;

   procedure Initialize_CPU_State (
      CPU_State       : out CPU_State_Type;
      For_Core_Thread :     Bool_Type)
   is
   begin
      CPU_Device_Pkg.Initialize_CPU_State (
         CPU_State, Bool_To_Ada (For_Core_Thread));
   end Initialize_CPU_State;

   procedure Initialize_MMU_Context (
      MMU_Context         : out MMU_Context_Type;
      Page_Table_Addr     :     Address_Type;
      Addr_Space_ID_Alloc :     Address_Type)
   is
      pragma Unreferenced (Addr_Space_ID_Alloc);
   begin
      CPU_Device_Pkg.Initialize_MMU_Context (MMU_Context, Page_Table_Addr);
   end Initialize_MMU_Context;

   procedure Start_Initializing_CPU_Device (
      CPU_Device : out CPU_Device_Type)
   is
   begin
      CPU_Device_Pkg.Start_Initializing_CPU_Device (CPU_Device);
   end Start_Initializing_CPU_Device;

   procedure Finish_Initializing_CPU_Device (
      CPU_Device : CPU_Device_Type)
   is
   begin
      CPU_Device_Pkg.Finish_Initializing_CPU_Device (CPU_Device);
   end Finish_Initializing_CPU_Device;

end CPP_CPU_Device_Pkg;
