--
--  \brief  Ada representation of C++ signal-data class
--  \author Martin stein
--  \date   2019-04-24
--

--
--  Copyright (C) 2019 Genode Labs GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

pragma Ada_2012;

with CPP;

package CPP_Signal_Data is

   type Object_Type is private;

   --
   --  Initialized_Object
   --
   function Initialized_Object (
      Impr    : CPP.Signal_Imprint_Type;
      Submits : CPP.Signal_Number_Of_Submits_Type)
   return Object_Type;

private

   type Object_Type is record
      Imprint       : CPP.Signal_Imprint_Type;
      Nr_Of_Submits : CPP.Signal_Number_Of_Submits_Type;
   end record;
   pragma Pack (Object_Type);

end CPP_Signal_Data;
