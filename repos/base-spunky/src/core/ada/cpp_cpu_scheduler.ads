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

with CPP;
with CPU_Scheduler;

package CPP_CPU_Scheduler
is
   --
   --  Share_Size
   --
   function Share_Size (Share : CPU_Scheduler.Share_Type)
   return CPP.Uint32_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel11object_sizeERKNS_9Cpu_shareE";

   --
   --  Scheduler_Size
   --
   function Scheduler_Size (Scheduler : CPU_Scheduler.Scheduler_Type)
   return CPP.Uint32_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel11object_sizeERKNS_13Cpu_schedulerE";

   --
   --  Share_Initialize
   --
   procedure Share_Initialize (
      Share : CPU_Scheduler.Share_Reference_Type;
      Prio  : CPP.CPU_Priority_Type;
      Quota : CPP.CPU_Quota_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel9Cpu_shareC2Eij";

   --
   --  Share_Ready
   --
   function Share_Ready (Share : CPU_Scheduler.Share_Type)
   return CPP.Bool_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZNK6Kernel9Cpu_share5readyEv";

   --
   --  Share_Quota
   --
   procedure Share_Quota (
      Share : in out CPU_Scheduler.Share_Type;
      Quota :        CPP.CPU_Quota_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel9Cpu_share5quotaEj";

   --
   --  Initialize
   --
   procedure Initialize (
      Scheduler : out CPU_Scheduler.Scheduler_Type;
      Idle      :     CPU_Scheduler.Share_Reference_Type;
      Quota     :     CPP.CPU_Quota_Type;
      Fill      :     CPP.CPU_Quota_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel13Cpu_schedulerC1ERNS_9Cpu_shareEjj";

   --
   --  Head
   --
   function Head (Scheduler : CPU_Scheduler.Scheduler_Type)
   return CPU_Scheduler.Share_Reference_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZNK6Kernel13Cpu_scheduler4headEv";

   --
   --  Head_Quota
   --
   function Head_Quota (Scheduler : CPU_Scheduler.Scheduler_Type)
   return CPP.CPU_Quota_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZNK6Kernel13Cpu_scheduler10head_quotaEv";

   --
   --  Need_To_Schedule
   --
   function Need_To_Schedule (Scheduler : CPU_Scheduler.Scheduler_Type)
   return CPP.Bool_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel13Cpu_scheduler16need_to_scheduleEv";

   --
   --  Timeout
   --
   procedure Timeout (Scheduler : in out CPU_Scheduler.Scheduler_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel13Cpu_scheduler7timeoutEv";

   --
   --  Quota
   --
   procedure Quota (
      Scheduler : in out CPU_Scheduler.Scheduler_Type;
      Share     :        CPU_Scheduler.Share_Reference_Type;
      Quota     :        CPP.CPU_Quota_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel13Cpu_scheduler5quotaERNS_9Cpu_shareEj";

   --
   --  Insert
   --
   procedure Insert (
      Scheduler : in out CPU_Scheduler.Scheduler_Type;
      Share     :        CPU_Scheduler.Share_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel13Cpu_scheduler6insertERNS_9Cpu_shareE";

   --
   --  Remove
   --
   procedure Remove (
      Scheduler : in out CPU_Scheduler.Scheduler_Type;
      Share     :        CPU_Scheduler.Share_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel13Cpu_scheduler6removeERNS_9Cpu_shareE";

   --
   --  Yield
   --
   procedure Yield (Scheduler : in out CPU_Scheduler.Scheduler_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel13Cpu_scheduler5yieldEv";

   --
   --  Unready
   --
   procedure Unready (
      Scheduler : in out CPU_Scheduler.Scheduler_Type;
      Share     :        CPU_Scheduler.Share_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel13Cpu_scheduler7unreadyERNS_9Cpu_shareE";

   --
   --  Ready
   --
   procedure Ready (
      Scheduler : in out CPU_Scheduler.Scheduler_Type;
      Share     :        CPU_Scheduler.Share_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel13Cpu_scheduler5readyERNS_9Cpu_shareE";

   --
   --  Ready_Check
   --
   procedure Ready_Check (
      Scheduler : in out CPU_Scheduler.Scheduler_Type;
      Share_1   :        CPU_Scheduler.Share_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN6Kernel13Cpu_scheduler11ready_checkERNS_9Cpu_shareE";

   --
   --  Update
   --
   procedure Update (
      Scheduler : in out CPU_Scheduler.Scheduler_Type;
      Time      :        CPP.Time_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel13Cpu_scheduler6updateEy";

end CPP_CPU_Scheduler;
