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

end Log;
