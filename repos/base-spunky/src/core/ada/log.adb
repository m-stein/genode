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
   --  Print_String_And_U64
   --
   procedure Print_String_And_U64 (
      Str : String;
      U64 : Unsigned_64)
   is
   begin
      Print_String_With_Length_And_U64 (Str, Str'Length, U64);
   end Print_String_And_U64;

   --
   --  Print_String_And_Addr
   --
   procedure Print_String_And_Addr (
      Str  : String;
      Addr : System.Address)
   is
   begin
      Print_String_With_Length_And_Addr (Str, Str'Length, Addr);
   end Print_String_And_Addr;

end Log;
