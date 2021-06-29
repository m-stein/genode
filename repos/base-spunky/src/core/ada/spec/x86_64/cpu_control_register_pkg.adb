--
--  \brief  Utility for accessing the CPUs control registers (CRx)
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

with System.Machine_Code; use System.Machine_Code;
with Interfaces;          use Interfaces;

package body CPU_Control_Register_Pkg is

   function Read
   return CR_Type
   is
      CR_U64 : Unsigned_64;
      CR     : CR_Type with Address => CR_U64'Address;
      pragma Assert (CR_U64'Size = CR'Size);
   begin

      Asm (
         "mov %%cr" & CR_Index & ", %0",
         Outputs => Unsigned_64'Asm_Output ("=r", CR_U64),
         Volatile => True);

      return CR;

   end Read;

   procedure Write (
      CR : CR_Type)
   is
      CR_U64 : Unsigned_64 with Address => CR'Address;
      pragma Assert (CR_U64'Size = CR'Size);
   begin

      Asm (
         "mov %0, %%cr" & CR_Index,
         Inputs => Unsigned_64'Asm_Input ("r", CR_U64),
         Volatile => True);

   end Write;

end CPU_Control_Register_Pkg;
