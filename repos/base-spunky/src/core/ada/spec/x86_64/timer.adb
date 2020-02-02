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

package body Timer is

   --
   --  Initialize
   --
   procedure Initialize (Tmr : out Timer_Type) is
   begin
      LVT_Entry.Vector := 32;
      LVT_Entry.Delivery := Normal;
      LVT_Entry.Mask := Disable;
      LVT_Entry.Mode := One_Shot;

      --  for Div of reverse Divide_Configuration_Value_Type loop
      --     exit loop when Ticks_Per_MS < Min_Ticks_Per_MS;
      --     Divide_Configuration.Value_
      --  end loop;

      Tmr.Ticks_Per_MS := 0;
      Tmr.PIT_Calc_Timer_Freq := 0;
   end Initialize;

end Timer;
