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

with System.Storage_Elements;

package body CPP_Architecture is

   --
   --  Round_Up_Address
   --
   function Round_Up_Address (
      Address               : System.Address;
      Byte_Granularity_Log2 : Unsigned_64)
   return System.Address
   is (
      U64_To_Addr (
         Round_Up_U64 (Addr_To_U64 (Address), Byte_Granularity_Log2)));

   --
   --  Round_Up_U64
   --
   function Round_Up_U64 (
      Value            : Unsigned_64;
      Granularity_Log2 : Unsigned_64)
   return Unsigned_64
   is
      Addend : constant Unsigned_64 :=
         Shift_Left (Unsigned_64 (1), Integer (Granularity_Log2)) - 1;

      Mask : constant Unsigned_64 := not Addend;
   begin

      return (Value + Addend) and Mask;

   end Round_Up_U64;

   --
   --  Addr_To_U64
   --
   function Addr_To_U64 (
      Address : System.Address)
   return Unsigned_64
   is (
      Unsigned_64 (System.Storage_Elements.To_Integer (Address)));

   --
   --  U64_To_Addr
   --
   function U64_To_Addr (
      U64 : Unsigned_64)
   return System.Address
   is (
      System.Storage_Elements.To_Address (
         System.Storage_Elements.Integer_Address (U64)));

end CPP_Architecture;
