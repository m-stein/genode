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

with System;
with CPP_Architecture;
with CPP;

use CPP_Architecture;
use CPP;

package Log is

   --
   --  Print_String_With_Length
   --
   procedure Print_String_With_Length (
      Str    : String;
      Length : Unsigned_Type)
   with
      Import,
      Convention => C,
      External_Name => "print_string_with_length";

   --
   --  Print_String_With_Length_And_UInt64
   --
   procedure Print_String_With_Length_And_UInt64 (
      Str    : String;
      Length : Unsigned_Type;
      Uint64 : Uint64_Type)
   with
      Import,
      Convention => C,
      External_Name => "print_string_with_length_and_uint64";

   --
   --  Print_String_With_Length_And_Address
   --
   procedure Print_String_With_Length_And_Address (
      Str    : String;
      Length : Unsigned_Type;
      Addr   : System.Address)
   with
      Import,
      Convention => C,
      External_Name => "print_string_with_length_and_address";

   --
   --  Print_UInt64
   --
   procedure Print_UInt64 (
      Uint64 : Uint64_Type)
   with
      Import,
      Convention => C,
      External_Name => "print_uint64";

   --
   --  Print_Address
   --
   procedure Print_Address (
      Addr : System.Address)
   with
      Import,
      Convention => C,
      External_Name => "print_address";

   --
   --  Print_String
   --
   procedure Print_String (Str : String);

   --
   --  Print_String_And_UInt64
   --
   procedure Print_String_And_UInt64 (
      Str    : String;
      Uint64 : Uint64_Type);

   --
   --  Print_String_And_Address
   --
   procedure Print_String_And_Address (
      Str  : String;
      Addr : System.Address);

end Log;
