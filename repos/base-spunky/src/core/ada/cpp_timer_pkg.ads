--
--  \brief  Glue code between Ada and C++ interfaces of timing classes
--  \author Martin stein
--  \date   2021-05-20
--

--
--  Copyright (C) 2021 Genode Labs GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

pragma Ada_2012;

with CPP_Thread;

with CPP;              use CPP;
with CPP_Architecture; use CPP_Architecture;
with Timer_Pkg;        use Timer_Pkg;

package CPP_Timer_Pkg is

   function Timer_Size (Timer : Timer_Type)
   return Size_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel11object_sizeERKNS_5TimerE";

   function Timeout_Size (Timeout : Timeout_Type)
   return Size_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel11object_sizeERKNS_7TimeoutE";

   function Interrupt_ID (
      Timer : Timer_Reference_Type)
   return Unsigned_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZNK6Kernel5Timer12interrupt_idEv";

   function Timeout_Max_US (
      Timer : Timer_Reference_Type)
   return Time_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZNK6Kernel5Timer14timeout_max_usEv";

   function US_To_Ticks (
      Timer : Timer_Reference_Type;
      US    : Time_Type)
   return Time_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZNK6Kernel5Timer11us_to_ticksEy";

   function Ticks_To_US (
      Timer : Timer_Reference_Type;
      Ticks : Time_Type)
   return Time_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZNK6Kernel5Timer11ticks_to_usEy";

   procedure Set_Timeout (
      Timer    : Timer_Reference_Type;
      Timeout  : Timeout_Reference_Type;
      Duration : Time_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel5Timer11set_timeoutEPNS_7TimeoutEy";

   function Current_Time (
      Timer : Timer_Reference_Type)
   return Time_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZNK6Kernel5Timer4timeEv";

   procedure Initialize_Timeout (
      Timeout : Timeout_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel7TimeoutC1Ev";

   procedure Initialize_Thread_Timeout (
      Timeout : Timeout_Reference_Type;
      Thread  : CPP_Thread.Object_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel7TimeoutC1ERNS_6ThreadE";

   procedure Process_Timeouts (
      Timer : Timer_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel5Timer16process_timeoutsEv";

   procedure Schedule_Timeout (
      Timer : Timer_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel5Timer16schedule_timeoutEv";

   function Time_Between_Schedule_Calls (
      Timer : Timer_Reference_Type)
   return Time_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZNK6Kernel5Timer27time_between_schedule_callsEv";

   procedure Initialize_Timer (
      Timer : Timer_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel5Timer11_initializeEv";

end CPP_Timer_Pkg;
