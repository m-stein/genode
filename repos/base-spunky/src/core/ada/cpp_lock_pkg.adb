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

with Interfaces; use Interfaces;

package body CPP_Lock_Pkg is

   function Lock_Size (Lock : Lock_Type)
   return Size_Type
   is (
      Lock'Size / 8);

   function Get_Kernel_Data_Lock
   return Lock_Reference_Type
   is (
      Lock_Pkg.Get_Kernel_Data_Lock);

   procedure Initialize_Lock_Pkg
   is
   begin
      Lock_Pkg.Initialize_Lock_Pkg;
   end Initialize_Lock_Pkg;

   procedure Lock (
      Lock : in out Lock_Type)
   is
   begin
      Lock_Pkg.Lock (Lock);
   end Lock;

   procedure Unlock (
      Lock : in out Lock_Type)
   is
   begin
      Lock_Pkg.Unlock (Lock);
   end Unlock;

end CPP_Lock_Pkg;
