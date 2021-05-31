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

package body Timer_Pkg is

   --
   --  Initialize_Timer
   --
   procedure Initialize_Timer (
      Timer : Timer_Reference_Type)
   is
   begin

      Timer_Device.Initialize (Timer.Device);
      Timer.Time := 0;
      Timer.Last_Timeout_Duration :=
         Timer_Device.Ticks_To_US (Timer.Device, Timer_Device.Ticks_Type'Last);

      Timer.Time_Between_Schedule_Calls := 0;
      Timeout_List_Pkg.Initialize (Timer.Timeout_List);

      --
      --  The timer frequency should allow a good accuracy on the smallest
      --  timeout syscall value (1 us).
      --
      if Timer_Device.Ticks_To_US (Timer.Device, 1) >= 1 and then
         Timer_Device.Ticks_To_US (
            Timer.Device, Timer_Device.Ticks_Type'Last) /=
               Time_Type (Timer_Device.Ticks_Type'Last)
      then
         raise Program_Error;
      end if;

      --
      --  The maximum measurable timeout is also the maximum age of a timeout
      --  installed by the timeout syscall. The timeout-age syscall returns a
      --  bogus value for older timeouts. A user that awoke from waiting for a
      --  timeout might not be schedulable in the same super period anymore.
      --  However, if the user can't manage to read the timeout age during the
      --  next super period, it's a bad configuration or the users fault. That
      --  said, the maximum timeout should be at least two times the super
      --  period).
      --
      if Timer_Device.Ticks_To_US (
            Timer.Device, Timer_Device.Ticks_Type'Last) <=
               2 * CPU_Quota_US
      then
         raise Program_Error;
      end if;

   end Initialize_Timer;

   --
   --  Initialize_Thread_Timeout
   --
   procedure Initialize_Thread_Timeout (
      Timeout : Timeout_Reference_Type;
      Thread  : CPP_Thread.Object_Reference_Type)
   is
   begin

      Timeout.Handler  := Thread_Handler;
      Timeout.Thread   := CPP_Thread.Object_Pointer_Type (Thread);
      Timeout_List_Pkg.Item_Initialize (Timeout.List_Item, Timeout);
      Timeout.Listed   := False;
      Timeout.Deadline := 0;

   end Initialize_Thread_Timeout;

   --
   --  Initialize_Timeout
   --
   procedure Initialize_Timeout (
      Timeout : Timeout_Reference_Type)
   is
   begin

      Timeout.Handler  := None;
      Timeout.Thread   := null;
      Timeout_List_Pkg.Item_Initialize (Timeout.List_Item, Timeout);
      Timeout.Listed   := False;
      Timeout.Deadline := 0;

   end Initialize_Timeout;

   --
   --  Timeout_Handle
   --
   procedure Timeout_Handle (
      Timeout : Timeout_Reference_Type)
   is
   begin

      if Timeout.Handler = Thread_Handler then
         CPP_Thread.Handle_Timeout (
            CPP_Thread.Object_Reference_Type (Timeout.Thread));
      end if;

   end Timeout_Handle;

   --
   --  Schedule_Timeout
   --
   procedure Schedule_Timeout (
      Timer : Timer_Reference_Type)
   is
      use Timeout_List_Pkg;
      List_Item : constant Timeout_List_Pkg.Item_Pointer_Type :=
         Timeout_List_Pkg.Head (Timer.Timeout_List);
   begin

      --
      --  Get the timeout with the nearest deadline.
      --
      if List_Item = null then
         raise Program_Error;
      end if;

      Declare_Timeout :
      declare
         Timeout : constant Timeout_Reference_Type :=
            Timeout_List_Pkg.Item_Payload (
               Timeout_List_Pkg.Item_Reference_Type (List_Item));
      begin

         Timer.Time_Between_Schedule_Calls :=
            Timer_Device.Duration (Timer.Device, Timer.Last_Timeout_Duration);

         Timer.Time := Timer.Time + Timer.Time_Between_Schedule_Calls;

         if Timeout.Deadline > Timer.Time then
            Timer.Last_Timeout_Duration := Timeout.Deadline - Timer.Time;
         else
            Timer.Last_Timeout_Duration := 1;
         end if;

         Timer_Device.Start_One_Shot (Timer.Last_Timeout_Duration);

      end Declare_Timeout;

   end Schedule_Timeout;

   --
   --  Set_Timeout
   --
   procedure Set_Timeout (
      Timer    : Timer_Reference_Type;
      Timeout  : Timeout_Reference_Type;
      Duration : Time_Type)
   is
      use Timeout_List_Pkg;
      Next_Shorter_Timeout : Timeout_Pointer_Type := null;
   begin

      --
      --  Remove timeout if it is already in use. Timeouts may get overridden
      --  as result of an update.
      --
      if  Timeout.Listed then
         Timeout_List_Pkg.Remove (
            Timer.Timeout_List, Timeout.List_Item'Access);
      else
         Timeout.Listed := True;
      end if;

      --
      --  Set timeout parameters.
      --
      Timeout.Deadline := Current_Time (Timer) + Duration;

      --
      --  Insert timeout. Timeouts are ordered ascending according to their
      --  deadline to be able to quickly determine the nearest timeout.
      --
      Declare_List_Item :
      declare
         Curr_List_Item : Timeout_List_Pkg.Item_Pointer_Type :=
            Timeout_List_Pkg.Head (Timer.Timeout_List);
      begin

         For_Each_Timeout_List_Item :
         while Curr_List_Item /= null loop

            Declare_Curr_Timeout :
            declare
               Curr_Timeout : constant Timeout_Reference_Type :=
                  Timeout_List_Pkg.Item_Payload (
                     Timeout_List_Pkg.Item_Reference_Type (Curr_List_Item));
            begin

               if Curr_Timeout.Deadline >= Timeout.Deadline then
                  exit For_Each_Timeout_List_Item;
               end if;
               Next_Shorter_Timeout := Timeout_Pointer_Type (Curr_Timeout);

            end Declare_Curr_Timeout;
            Curr_List_Item :=
               Timeout_List_Pkg.Item_Next (
                  Timeout_List_Pkg.Item_Reference_Type (Curr_List_Item));

         end loop For_Each_Timeout_List_Item;

      end Declare_List_Item;

      if Next_Shorter_Timeout = null then

         Timeout_List_Pkg.Insert_Head (
            Timer.Timeout_List, Timeout.List_Item'Access);

      else

         Timeout_List_Pkg.Insert_Behind (
            Timer.Timeout_List,
            Timeout.List_Item'Access,
            Next_Shorter_Timeout.List_Item'Access);

      end if;

   end Set_Timeout;

   --
   --  Process_Timeouts
   --
   procedure Process_Timeouts (
      Timer : Timer_Reference_Type)
   is
      use Timeout_List_Pkg;
      Curr_Time : constant Time_Type := Current_Time (Timer);
   begin

      Declare_List_Item :
      declare
         Curr_List_Item : Timeout_List_Pkg.Item_Pointer_Type :=
            Timeout_List_Pkg.Head (Timer.Timeout_List);
      begin

         For_Each_Timeout_List_Item :
         while Curr_List_Item /= null loop

            Declare_Curr_Timeout :
            declare
               Curr_Timeout : constant Timeout_Reference_Type :=
                  Timeout_List_Pkg.Item_Payload (
                     Timeout_List_Pkg.Item_Reference_Type (Curr_List_Item));
            begin

               if Curr_Timeout.Deadline > Curr_Time then
                  exit For_Each_Timeout_List_Item;
               end if;

               Timeout_List_Pkg.Remove (
                  Timer.Timeout_List,
                  Timeout_List_Pkg.Item_Reference_Type (Curr_List_Item));

               Curr_Timeout.Listed := False;
               Timeout_Handle (Curr_Timeout);
               Curr_List_Item :=
                  Timeout_List_Pkg.Item_Next (
                     Timeout_List_Pkg.Item_Reference_Type (Curr_List_Item));

            end Declare_Curr_Timeout;

         end loop For_Each_Timeout_List_Item;

      end Declare_List_Item;

   end Process_Timeouts;

   --
   --  Time_Between_Schedule_Calls
   --
   function Time_Between_Schedule_Calls (
      Timer : Timer_Reference_Type)
   return Time_Type
   is (Timer.Time_Between_Schedule_Calls);

   --
   --  Timeout_Max_US
   --
   function Timeout_Max_US (
      Timer : Timer_Reference_Type)
   return Time_Type
   is (Timer_Device.Ticks_To_US (Timer.Device, Timer_Device.Ticks_Type'Last));

   --
   --  Ticks_To_US
   --
   function Ticks_To_US (
      Timer : Timer_Reference_Type;
      Ticks : Time_Type)
   return Time_Type
   is (
      Timer_Device.Ticks_To_US (
         Timer.Device, Timer_Device.Ticks_Type (Ticks)));

   --
   --  US_To_Ticks
   --
   function US_To_Ticks (
      Timer : Timer_Reference_Type;
      US    : Time_Type)
   return Time_Type
   is (Timer_Device.US_To_Ticks (Timer.Device, US));

   --
   --  Interrupt_ID
   --
   function Interrupt_ID
   return IRQ_ID_Type
   is (Timer_Device.Interrupt_ID);

   --
   --  Current_Time
   --
   function Current_Time (
      Timer : Timer_Reference_Type)
   return Time_Type
   is (
      Timer.Time +
      Timer_Device.Duration (Timer.Device, Timer.Last_Timeout_Duration));

end Timer_Pkg;
