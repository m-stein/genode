--
--  \brief  Compound of all per-CPU objects in the kernel
--  \author Martin stein
--  \date   2021-06-09
--

--
--  Copyright (C) 2021 Genode Labs GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

pragma Ada_2012;

with Interfaces; use Interfaces;

package body CPU_Pkg is

   function CPU_Size (CPU : CPU_Type)
   return Size_Type
   is (CPU'Size / 8);

end CPU_Pkg;
