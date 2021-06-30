--
--  \brief  Glue code between Ada and C++ interface of CPU scheduler
--  \author Martin stein
--  \date   2020-01-05
--

--
--  Copyright (C) 2020 Genode Labs GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

pragma Ada_2012;

with Interfaces; use Interfaces;

package body CPP_CPU_Scheduler
is
   --
   --  Share_Size
   --
   function Share_Size (Share : CPU_Scheduler.Share_Type)
   return Size_Type
   is (Share'Size / 8);

   --
   --  Scheduler_Size
   --
   function Scheduler_Size (Scheduler : CPU_Scheduler.Scheduler_Type)
   return Size_Type
   is (Scheduler'Size / 8);

   --
   --  Share_Initialize
   --
   procedure Share_Initialize (
      Share : CPU_Scheduler.Share_Reference_Type;
      Prio  : CPU_Priority_Type;
      Quota : CPU_Quota_Type)
   is
   begin
      CPU_Scheduler.Share_Initialize (
         Share, CPU_Scheduler.Priority_Type (Prio),
         CPU_Scheduler.Quota_Type (Quota));

   end Share_Initialize;

   --
   --  Share_Ready
   --
   function Share_Ready (Share : CPU_Scheduler.Share_Type)
   return Bool_Type
   is (Bool_From_Ada (CPU_Scheduler.Share_Ready (Share)));

   --
   --  Share_Quota
   --
   procedure Share_Quota (
      Share : in out CPU_Scheduler.Share_Type;
      Quota :        CPU_Quota_Type)
   is
   begin
      CPU_Scheduler.Share_Quota (Share, CPU_Scheduler.Quota_Type (Quota));
   end Share_Quota;

   --
   --  Initialize
   --
   procedure Initialize (
      Scheduler : out CPU_Scheduler.Scheduler_Type;
      Idle      :     CPU_Scheduler.Share_Reference_Type;
      Quota     :     CPU_Quota_Type;
      Fill      :     CPU_Quota_Type)
   is
   begin
      CPU_Scheduler.Initialize (
         Scheduler, Idle, CPU_Scheduler.Quota_Type (Quota),
         CPU_Scheduler.Quota_Type (Fill));

   end Initialize;

   --
   --  Head
   --
   function Head (Scheduler : CPU_Scheduler.Scheduler_Type)
   return CPU_Scheduler.Share_Reference_Type
   is (CPU_Scheduler.Head (Scheduler));

   --
   --  Head_Quota
   --
   function Head_Quota (Scheduler : CPU_Scheduler.Scheduler_Type)
   return CPU_Quota_Type
   is (CPU_Quota_Type (CPU_Scheduler.Head_Quota (Scheduler)));

   --
   --  Need_To_Schedule
   --
   function Need_To_Schedule (Scheduler : CPU_Scheduler.Scheduler_Type)
   return Bool_Type
   is (Bool_From_Ada (CPU_Scheduler.Need_To_Schedule (Scheduler)));

   --
   --  Timeout
   --
   procedure Timeout (Scheduler : in out CPU_Scheduler.Scheduler_Type)
   is
   begin
      CPU_Scheduler.Timeout (Scheduler);
   end Timeout;

   --
   --  Quota
   --
   procedure Quota (
      Scheduler : in out CPU_Scheduler.Scheduler_Type;
      Share     :        CPU_Scheduler.Share_Reference_Type;
      Quota     :        CPU_Quota_Type)
   is
   begin
      CPU_Scheduler.Quota (
         Scheduler, Share, CPU_Scheduler.Quota_Type (Quota));

   end Quota;

   --
   --  Insert
   --
   procedure Insert (
      Scheduler : in out CPU_Scheduler.Scheduler_Type;
      Share     :        CPU_Scheduler.Share_Reference_Type)
   is
   begin
      CPU_Scheduler.Insert (Scheduler, Share);
   end Insert;

   --
   --  Remove
   --
   procedure Remove (
      Scheduler : in out CPU_Scheduler.Scheduler_Type;
      Share     :        CPU_Scheduler.Share_Reference_Type)
   is
   begin
      CPU_Scheduler.Remove (Scheduler, Share);
   end Remove;

   --
   --  Yield
   --
   procedure Yield (Scheduler : in out CPU_Scheduler.Scheduler_Type)
   is
   begin
      CPU_Scheduler.Yield (Scheduler);
   end Yield;

   --
   --  Unready
   --
   procedure Unready (
      Scheduler : in out CPU_Scheduler.Scheduler_Type;
      Share     :        CPU_Scheduler.Share_Reference_Type)
   is
   begin
      CPU_Scheduler.Unready (Scheduler, Share);
   end Unready;

   --
   --  Ready
   --
   procedure Ready (
      Scheduler : in out CPU_Scheduler.Scheduler_Type;
      Share     :        CPU_Scheduler.Share_Reference_Type)
   is
   begin
      CPU_Scheduler.Ready (Scheduler, Share);
   end Ready;

   --
   --  Ready_Check
   --
   procedure Ready_Check (
      Scheduler : in out CPU_Scheduler.Scheduler_Type;
      Share_1   :        CPU_Scheduler.Share_Reference_Type)
   is
   begin
      CPU_Scheduler.Ready_Check (Scheduler, Share_1);
   end Ready_Check;

   --
   --  Update
   --
   procedure Update (
      Scheduler : in out CPU_Scheduler.Scheduler_Type;
      Time      :        Time_Type)
   is
   begin
      CPU_Scheduler.Update (Scheduler, CPU_Scheduler.Time_Type (Time));
   end Update;

end CPP_CPU_Scheduler;
