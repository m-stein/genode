--
--  \brief  Log output
--  \author Martin stein
--  \date   2020-02-02
--

--
--  Copyright (C) 2020 Genode Labs GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

pragma Ada_2012;

package body Log is

   --
   --  Print_String
   --
   procedure Print_String (Str : String)
   is
   begin
      Print_String_With_Length (Str, Str'Length);
   end Print_String;

   --
   --  Print_String_And_UInt64
   --
   procedure Print_String_And_UInt64 (
      Str    : String;
      Uint64 : Uint64_Type)
   is
   begin
      Print_String_With_Length_And_UInt64 (Str, Str'Length, Uint64);
   end Print_String_And_UInt64;

   --
   --  Print_String_And_Address
   --
   procedure Print_String_And_Address (
      Str  : String;
      Addr : System.Address)
   is
   begin
      Print_String_With_Length_And_Address (Str, Str'Length, Addr);
   end Print_String_And_Address;

end Log;
