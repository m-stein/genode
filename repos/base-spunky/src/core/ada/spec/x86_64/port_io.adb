--
--  \brief  Utilities for performing port IO
--  \author Martin stein
--  \date   2020-02-06
--

--
--  Copyright (C) 2020 Genode Labs GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

pragma Ada_2012;

with System.Machine_Code;

package body Port_IO
is
   --
   --  Out_Byte
   --
   procedure Out_Byte (
      Port : Port_Type;
      Byte : Byte_Type)
   is
   begin
      System.Machine_Code.Asm (
         "outb %0, %w1",
         Inputs => (
            Byte_Type'Asm_Input ("a", Byte),
            Port_Type'Asm_Input ("Nd", Port)),
         Volatile => True);

   end Out_Byte;

   --
   --  In_Byte
   --
   function In_Byte (Port : Port_Type)
   return Byte_Type
   is
      Byte : Byte_Type;
   begin
      System.Machine_Code.Asm (
         "inb %w1, %0",
         Outputs => Byte_Type'Asm_Output ("=a", Byte),
         Inputs => Port_Type'Asm_Input ("Nd", Port),
         Volatile => True);

      return Byte;
   end In_Byte;

end Port_IO;
