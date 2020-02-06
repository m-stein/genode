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

with CPP_Architecture;

package Log is

   --
   --  Print_String_With_Length
   --
   procedure Print_String_With_Length (
      Str    : String;
      Length : CPP_Architecture.Unsigned_Type)
   with
      Import,
      Convention => C,
      External_Name => "print_string_with_length";

   --
   --  Print_String
   --
   procedure Print_String (Str : String);

end Log;
