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

package CPP
with Spark_Mode
is
   pragma Pure;

   type Bool_Type is range 0 .. 1 with Size => 8;

   function Bool_From_Ada (Value : Boolean)
   return Bool_Type;

end CPP;
