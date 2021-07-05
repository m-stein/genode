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

with CPP_Architecture; use CPP_Architecture;
with Interfaces;       use Interfaces;

package CPP is

   pragma Pure;

   type Bool_Type                     is range 0 .. 1         with Size => 8;
   type Byte_Type                     is range 0 .. 2**8 - 1  with Size => 8;
   type Signal_Imprint_Type           is new Address_Type;
   type Signal_Number_Of_Submits_Type is new Unsigned_Type;
   type Time_Type                     is new Unsigned_64;
   type CPU_Quota_Type                is new Unsigned_Type;
   type CPU_Priority_Type             is new Signed_Type;
   type CPU_ID_Type                   is new Unsigned_Type;
   type IRQ_ID_Type                   is new Unsigned_Type;
   type IRQ_Polarity_Type             is (Invalid, High, Low);
   type IRQ_Trigger_Mode_Type         is (Invalid, Edge, Level);
   type Number_Of_IRQs_Type           is new Unsigned_Type;
   type CPU_Index_Type                is range 0 .. 31;

   --
   --  Registers and bitfields
   --
   type Bitfield_1_Type  is range 0 .. 2**1 - 1;
   type Bitfield_2_Type  is range 0 .. 2**2 - 1;
   type Bitfield_3_Type  is range 0 .. 2**3 - 1;
   type Bitfield_4_Type  is range 0 .. 2**4 - 1;
   type Bitfield_5_Type  is range 0 .. 2**5 - 1;
   type Bitfield_6_Type  is range 0 .. 2**6 - 1;
   type Bitfield_7_Type  is range 0 .. 2**7 - 1;
   type Bitfield_8_Type  is range 0 .. 2**8 - 1;
   type Bitfield_24_Type is range 0 .. 2**24 - 1;
   type Bitfield_52_Type is range 0 .. 2**52 - 1;

   type Page_Fault_Reason_Type is (Unknown, Write, Execute, Page_Missing)
   with Size => 8;

   for Page_Fault_Reason_Type use (
      Unknown      => 0,
      Write        => 1,
      Execute      => 2,
      Page_Missing => 3);

   type Page_Fault_State_Type is record
      IP      : Address_Type;
      Address : Address_Type;
      Reason  : Page_Fault_Reason_Type;
   end record;

   --
   --  IRQ_Trigger_Mode_From_Unsigned
   --
   function IRQ_Trigger_Mode_From_Unsigned (Unsigned : Unsigned_Type)
   return IRQ_Trigger_Mode_Type;

   --
   --  IRQ_Polarity_From_Unsigned
   --
   function IRQ_Polarity_From_Unsigned (Unsigned : Unsigned_Type)
   return IRQ_Polarity_Type;

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
