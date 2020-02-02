--
--  \brief  Timer driver
--  \author Martin stein
--  \date   2020-02-02
--

--
--  Copyright (C) 2019 Genode Labs GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

pragma Ada_2012;

with System;

package Timer is

   type Timer_Type is private;

   --
   --  Initialize
   --
   procedure Initialize (Tmr : out Timer_Type);

private

   Local_APIC_LVT_Timer_Entry_Address : constant := 16#320#;

   type Ticks_Type is range 0 .. 32**2 - 1;

   type LVT_Entry_Vector_Type is range 0 .. 2**8 - 1;

   type LVT_Entry_Delivery_Type is (Normal);
   for LVT_Entry_Delivery_Type use (Normal => 0);

   type LVT_Entry_Mode_Type is (One_Shot);
   for LVT_Entry_Mode_Type use (One_Shot => 0);

   type LVT_Entry_Mask_Type is (Disable);
   for LVT_Entry_Mask_Type use (Disable => 0);

   type LVT_Entry_Type is record
      Vector : LVT_Entry_Vector_Type;
      Delivery : LVT_Entry_Delivery_Type;
      Mask : LVT_Entry_Mask_Type;
      Mode : LVT_Entry_Mode_Type;
   end record;

   for LVT_Entry_Type use
   record
      Vector at 0 range 0 .. 7;
      Delivery at 0 range 8 .. 10;
      Mask at 0 range 16 .. 16;
      Mode at 0 range 17 .. 18;
   end record;

   LVT_Entry : LVT_Entry_Type
   with Address => System'To_Address (Local_APIC_LVT_Timer_Entry_Address);

   type Timer_Type is record
      Ticks_Per_MS        : Ticks_Type;
      PIT_Calc_Timer_Freq : Ticks_Type;
   end record;

end Timer;
