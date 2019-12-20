--
--  \brief  General types and subprograms for the glue between Ada and C++
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

with CPP_Architecture;

package CPP is

   pragma Pure;

   type Bool_Type                     is range 0 .. 1         with Size => 8;
   type Byte_Type                     is range 0 .. 2**8 - 1  with Size => 8;
   type Uint32_Type                   is range 0 .. 2**32 - 1 with Size => 32;
   type Signal_Imprint_Type           is new CPP_Architecture.Address_Type;
   type Signal_Number_Of_Submits_Type is new CPP_Architecture.Unsigned_Type;

   --
   --  Bool_From_Ada
   --
   function Bool_From_Ada (Value : Boolean)
   return Bool_Type;

   --
   --  Bool_To_Ada
   --
   function Bool_To_Ada (Value : Bool_Type)
   return Boolean;

end CPP;
