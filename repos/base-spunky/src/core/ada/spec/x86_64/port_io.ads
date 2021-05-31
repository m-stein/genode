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

package Port_IO
is
   type Port_Type is range 0 .. 2**16 - 1;
   type Byte_Type is mod 2**8 with Size => 8;

   --
   --  Out_Byte
   --
   procedure Out_Byte (
      Port : Port_Type;
      Byte : Byte_Type);

   --
   --  In_Byte
   --
   function In_Byte (Port : Port_Type)
   return Byte_Type;

end Port_IO;
