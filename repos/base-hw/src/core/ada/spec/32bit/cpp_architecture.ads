--
--  \brief  Ada representation of architecture-dependent C++ types
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

package CPP_Architecture is

   pragma Pure;

   type Address_Type  is mod 2**32 with Size => 32;
   type Unsigned_Type is mod 2**32 with Size => 32

end CPP_Architecture;
