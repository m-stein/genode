--
--  \brief  Control mutual exclusion of multiple CPUs on shared kernel data
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

with CPU_Device_Pkg;

with Interfaces; use Interfaces;
with Log;        use Log;

package body Lock_Pkg is

   function Get_Kernel_Data_Lock
   return Lock_Reference_Type
   is (
      Kernel_Data_Lock'Access);

   procedure Initialize_Lock_Pkg
   is
   begin
      Kernel_Data_Lock.State        := Unlocked;
      Kernel_Data_Lock.CPU_ID_Valid := False;
      Kernel_Data_Lock.CPU_ID       := 0;

   end Initialize_Lock_Pkg;

   procedure Lock (
      Lock : in out Lock_Type)
   is
      State_U32       : aliased Unsigned_32 with Address => Lock.State'Address;
      State_Exchanged : Boolean := False;

      CPU_ID : constant CPU_ID_Type :=
         CPU_Device_Pkg.ID_Of_Executing_CPU;
   begin

      if Lock.CPU_ID_Valid and then
         Lock.CPU_ID = CPU_ID
      then
         Print_String (
            "Error: Re-entered lock. Might indicate that an exception was " &
            "thrown inside the kernel.");
      end if;

      while not State_Exchanged loop

         CPU_Device_Pkg.Atomic_Compare_And_Exchange_U32 (
            State_Exchanged, State_U32'Access, Unlocked'Enum_Rep,
            Locked'Enum_Rep);

      end loop;
      Lock.CPU_ID       := CPU_ID;
      Lock.CPU_ID_Valid := True;

   end Lock;

   procedure Unlock (
      Lock : in out Lock_Type)
   is
   begin
      Lock.CPU_ID_Valid := False;
      CPU_Device_Pkg.Memory_Barrier;
      Lock.State := Unlocked;

   end Unlock;

end Lock_Pkg;
