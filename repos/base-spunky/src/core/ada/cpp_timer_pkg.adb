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

package body CPP_Timer_Pkg is

   function Timer_Size (Timer : Timer_Type)
   return CPP.Uint32_Type
   is (CPP.Uint32_Type (Timer'Size / 8));

   function Timeout_Size (Timeout : Timeout_Type)
   return CPP.Uint32_Type
   is (CPP.Uint32_Type (Timeout'Size / 8));

   function Interrupt_ID (
      Timer : Timer_Reference_Type)
   return Unsigned_Type
   is
      pragma Unreferenced (Timer);
   begin
      return Unsigned_Type (Timer_Pkg.Interrupt_ID);
   end Interrupt_ID;

   function Timeout_Max_US (
      Timer : Timer_Reference_Type)
   return Time_Type
   is (Timer_Pkg.Timeout_Max_US (Timer));

   function Time_Between_Schedule_Calls (
      Timer : Timer_Reference_Type)
   return Time_Type
   is (Timer_Pkg.Time_Between_Schedule_Calls (Timer));

   function US_To_Ticks (
      Timer : Timer_Reference_Type;
      US    : Time_Type)
   return Time_Type
   is (Timer_Pkg.US_To_Ticks (Timer, US));

   function Ticks_To_US (
      Timer : Timer_Reference_Type;
      Ticks : Time_Type)
   return Time_Type
   is (Timer_Pkg.Ticks_To_US (Timer, Ticks));

   procedure Set_Timeout (
      Timer    : Timer_Reference_Type;
      Timeout  : Timeout_Reference_Type;
      Duration : Time_Type)
   is
   begin
      Timer_Pkg.Set_Timeout (Timer, Timeout, Duration);
   end Set_Timeout;

   function Current_Time (
      Timer : Timer_Reference_Type)
   return Time_Type
   is (Timer_Pkg.Current_Time (Timer));

   procedure Initialize_Timeout (
      Timeout : Timeout_Reference_Type)
   is
   begin
      Timer_Pkg.Initialize_Timeout (Timeout);
   end Initialize_Timeout;

   procedure Initialize_Thread_Timeout (
      Timeout : Timeout_Reference_Type;
      Thread  : CPP_Thread.Object_Reference_Type)
   is
   begin
      Timer_Pkg.Initialize_Thread_Timeout (Timeout, Thread);
   end Initialize_Thread_Timeout;

   procedure Process_Timeouts (
      Timer : Timer_Reference_Type)
   is
   begin
      Timer_Pkg.Process_Timeouts (Timer);
   end Process_Timeouts;

   procedure Schedule_Timeout (
      Timer : Timer_Reference_Type)
   is
   begin
      Timer_Pkg.Schedule_Timeout (Timer);
   end Schedule_Timeout;

   procedure Initialize_Timer (
      Timer : Timer_Reference_Type)
   is
   begin
      Timer_Pkg.Initialize_Timer (Timer);
   end Initialize_Timer;

end CPP_Timer_Pkg;
