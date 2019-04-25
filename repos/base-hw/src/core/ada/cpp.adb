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

package body CPP is

   function Bool_From_Ada (Value : Boolean)
   return Bool_Type
   is (
      case Value is
      when False => 0,
      when True  => 1);

   function Bool_To_Ada (Value : Bool_Type)
   return Boolean
   is (
      case Value is
      when 0 => False,
      when 1 => True);

end CPP;
