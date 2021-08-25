--
--  \brief  Glue code between Ada and C++ interface of the lock package
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

with CPP_Architecture; use CPP_Architecture;
with Lock_Pkg;         use Lock_Pkg;

package CPP_Lock_Pkg is

   function Lock_Size (Lock : Lock_Type)
   return Size_Type
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZNK6Kernel15Opaque_ada_typeINS_4LockELm12EE9_ada_sizeEv";

   procedure Initialize (
      Lock : out Lock_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel4LockC1Ev";

   procedure Lock (
      Lock : in out Lock_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel4Lock4lockEv";

   procedure Unlock (
      Lock : in out Lock_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel4Lock6unlockEv";

end CPP_Lock_Pkg;
