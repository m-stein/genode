--
--  \brief  Kernel timer that multiplexes timeouts on a single timer device
--  \author Martin stein
--  \date   2021-05-12
--

--
--  Copyright (C) 2021 Genode Labs GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

pragma Ada_2012;

with CPP;
with CPP_Thread;
with Timer_Device;
with Generic_Double_List;

use CPP;

package Timer_Pkg is

   CPU_Quota_US : constant := 1_000_000;

   type Timer_Type           is private;
   type Timer_Reference_Type is not null access all Timer_Type;
   type Timer_Pointer_Type   is access all Timer_Type;

   type Timeout_Type           is private;
   type Timeout_Reference_Type is not null access all Timeout_Type;
   type Timeout_Pointer_Type   is access all Timeout_Type;

   --
   --  Initialize_Timer
   --
   procedure Initialize_Timer (
      Timer : Timer_Reference_Type);

   --
   --  Initialize_Thread_Timeout
   --
   procedure Initialize_Thread_Timeout (
      Timeout : Timeout_Reference_Type;
      Thread  : CPP_Thread.Object_Reference_Type);

   --
   --  Initialize_Timeout
   --
   procedure Initialize_Timeout (
      Timeout : Timeout_Reference_Type);

   --
   --  Schedule_Timeout
   --
   procedure Schedule_Timeout (
      Timer : Timer_Reference_Type);

   --
   --  Set_Timeout
   --
   procedure Set_Timeout (
      Timer    : Timer_Reference_Type;
      Timeout  : Timeout_Reference_Type;
      Duration : Time_Type);

   --
   --  Process_Timeouts
   --
   procedure Process_Timeouts (
      Timer : Timer_Reference_Type);

   --
   --  Time_Between_Schedule_Calls
   --
   function Time_Between_Schedule_Calls (
      Timer : Timer_Reference_Type)
   return Time_Type;

   --
   --  Timeout_Max_US
   --
   function Timeout_Max_US (
      Timer : Timer_Reference_Type)
   return Time_Type;

   --
   --  Current_Time
   --
   function Current_Time (
      Timer : Timer_Reference_Type)
   return Time_Type;

   --
   --  Ticks_To_US
   --
   function Ticks_To_US (
      Timer : Timer_Reference_Type;
      Ticks : Time_Type)
   return Time_Type;

   --
   --  Interrupt_ID
   --
   function Interrupt_ID
   return IRQ_ID_Type;

   --
   --  US_To_Ticks
   --
   function US_To_Ticks (
      Timer : Timer_Reference_Type;
      US    : Time_Type)
   return Time_Type;

private

   package Timeout_List_Pkg is new Generic_Double_List (
      Timeout_Type,
      Timeout_Reference_Type);

   --
   --  Timeout_Handle
   --
   procedure Timeout_Handle (
      Timeout : Timeout_Reference_Type);

   type Timeout_Handler_Type is (Thread_Handler, None);

   type Timeout_Type is record
      Handler   : Timeout_Handler_Type;
      Thread    : CPP_Thread.Object_Pointer_Type;
      List_Item : aliased Timeout_List_Pkg.Item_Type;
      Listed    : Boolean;
      Deadline  : Time_Type;
   end record;

   type Timer_Type is record
      Device                      : Timer_Device.Timer_Device_Type;
      Time                        : Time_Type;
      Last_Timeout_Duration       : Time_Type;
      Time_Between_Schedule_Calls : Time_Type;
      Timeout_List                : Timeout_List_Pkg.List_Type;
   end record;

end Timer_Pkg;
