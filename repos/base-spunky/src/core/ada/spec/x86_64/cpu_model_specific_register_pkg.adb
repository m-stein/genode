--
--  \brief  Utility for accessing the CPUs model-specific registers (MSR)
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

with Interfaces;          use Interfaces;
with System.Machine_Code; use System.Machine_Code;

package body CPU_Model_Specific_Register_Pkg is

   function Read
   return MSR_Type
   is
      MSR_Low_U32  : Unsigned_32;
      MSR_High_U32 : Unsigned_32;
      MSR_U64      : Unsigned_64;
      MSR          : MSR_Type with Address => MSR_U64'Address;
      pragma Assert (MSR_U64'Size = MSR'Size);
   begin

      Asm (
         "rdmsr",
         Inputs => (
            Unsigned_32'Asm_Input ("c", Unsigned_32 (MSR_Address))),
         Outputs => (
            Unsigned_32'Asm_Output ("=a", MSR_Low_U32),
            Unsigned_32'Asm_Output ("=d", MSR_High_U32)),
         Volatile => True);

      MSR_U64 :=
         Shift_Left (Unsigned_64 (MSR_High_U32), 32) or
         Unsigned_64 (MSR_Low_U32);

      return MSR;

   end Read;

   procedure Write (
      MSR : MSR_Type)
   is
      MSR_U64 : Unsigned_64 with Address => MSR'Address;
      pragma Assert (MSR_U64'Size = MSR'Size);

      MSR_Low_U32  : constant Unsigned_32 := Unsigned_32 (MSR_U64);
      MSR_High_U32 : constant Unsigned_32 :=
         Unsigned_32 (Shift_Right (MSR_U64, 32));
   begin

      Asm (
         "wrmsr",
         Inputs => (
            Unsigned_32'Asm_Input ("a", MSR_Low_U32),
            Unsigned_32'Asm_Input ("d", MSR_High_U32),
            Unsigned_32'Asm_Input ("c", Unsigned_32 (MSR_Address))),
         Volatile => True);

   end Write;

end CPU_Model_Specific_Register_Pkg;
