--
--  \brief  Scheduling of execution time of the CPU
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

with Generic_Double_List;

package CPU_Scheduler
is
   Nr_Of_Priorities : constant := 4;

   type Scheduler_Type is private;
   type Share_Type is private;
   type Share_Reference_Type is not null access all Share_Type;
   type Priority_Type is range 0 .. Nr_Of_Priorities - 1;
   type Quota_Type is range 0 .. 2**32 - 1;
   type Time_Type is mod 2**64;

   --
   --  Share_Initialize
   --
   procedure Share_Initialize (
      Share : Share_Reference_Type;
      Prio  : Priority_Type;
      Quota : Quota_Type);

   --
   --  Share_Ready
   --
   function Share_Ready (Share : Share_Type)
   return Boolean;

   --
   --  Share_Quota
   --
   procedure Share_Quota (
      Share : in out Share_Type;
      Quota :        Quota_Type);

   --
   --  Initialize
   --
   procedure Initialize (
      Scheduler : out Scheduler_Type;
      Idle      :     Share_Reference_Type;
      Quota     :     Quota_Type;
      Fill      :     Quota_Type);

   --
   --  Head
   --
   function Head (Scheduler : Scheduler_Type)
   return Share_Reference_Type;

   --
   --  Head_Quota
   --
   function Head_Quota (Scheduler : Scheduler_Type)
   return Quota_Type;

   --
   --  Need_To_Schedule
   --
   function Need_To_Schedule (Scheduler : Scheduler_Type)
   return Boolean;

   --
   --  Timeout
   --
   procedure Timeout (Scheduler : in out Scheduler_Type);

   --
   --  Quota
   --
   procedure Quota (
      Scheduler : in out Scheduler_Type;
      Share     :        Share_Reference_Type;
      Quota     :        Quota_Type);

   --
   --  Insert
   --
   procedure Insert (
      Scheduler : in out Scheduler_Type;
      Share     :        Share_Reference_Type);

   --
   --  Remove
   --
   procedure Remove (
      Scheduler : in out Scheduler_Type;
      Share     :        Share_Reference_Type);

   --
   --  Yield
   --
   procedure Yield (Scheduler : in out Scheduler_Type);

   --
   --  Unready
   --
   procedure Unready (
      Scheduler : in out Scheduler_Type;
      Share     :        Share_Reference_Type);

   --
   --  Ready
   --
   procedure Ready (
      Scheduler : in out Scheduler_Type;
      Share     :        Share_Reference_Type);

   --
   --  Ready_Check
   --
   procedure Ready_Check (
      Scheduler : in out Scheduler_Type;
      Share_1   :        Share_Reference_Type);

   --
   --  Update
   --
   procedure Update (
      Scheduler : in out Scheduler_Type;
      Time      :        Time_Type);

private

   package Share_List is new Generic_Double_List (
      Share_Type,
      Share_Reference_Type);

   type Share_Pointer_Type is access all Share_Type;
   type Claim_Lists_Type is array (Priority_Type) of Share_List.List_Type;

   type Scheduler_Type is record
      Claims_Ready     : Claim_Lists_Type;
      Claims_Unready   : Claim_Lists_Type;
      Fills            : Share_List.List_Type;
      Idle             : Share_Reference_Type;
      Head             : Share_Pointer_Type;
      Head_Quota       : Quota_Type;
      Head_Claims      : Boolean;
      Head_Yields      : Boolean;
      Quota            : Quota_Type;
      Residual         : Quota_Type;
      Fill             : Quota_Type;
      Need_To_Schedule : Boolean;
      Last_Time        : Time_Type;
   end record;

   type Share_Type is record
      Fill_Item  : aliased Share_List.Item_Type;
      Claim_Item : aliased Share_List.Item_Type;
      Prio       : Priority_Type;
      Quota      : Quota_Type;
      Claim      : Quota_Type;
      Fill       : Quota_Type;
      Ready      : Boolean;
   end record;

   --
   --  Quota_Adaption
   --
   procedure Quota_Adaption (
      Scheduler : in out Scheduler_Type;
      Share     :        Share_Reference_Type;
      Quota     :        Quota_Type);

   --
   --  Quota_Introduction
   --
   procedure Quota_Introduction (
      Scheduler : in out Scheduler_Type;
      Share     :        Share_Reference_Type);

   --
   --  Set_Head
   --
   procedure Set_Head (
      Scheduler : in out Scheduler_Type;
      Share     :        Share_Reference_Type;
      Quota     :        Quota_Type;
      Claims    :        Boolean);

   --
   --  Trim_Consumption
   --
   procedure Trim_Consumption (
      Scheduler : in out Scheduler_Type;
      Quota     : in out Quota_Type;
      Residual  :    out Quota_Type);

   --
   --  Fill_For_Head
   --
   procedure Fill_For_Head (
      Scheduler  : in out Scheduler_Type;
      Fill_Found :    out Boolean);

   --
   --  Claim_For_Head
   --
   procedure Claim_For_Head (
      Scheduler   : in out Scheduler_Type;
      Claim_Found :    out Boolean);

   --
   --  Head_Filled
   --
   procedure Head_Filled (
      Scheduler : in out Scheduler_Type;
      Residual  :        Quota_Type);

   --
   --  Head_Claimed
   --
   procedure Head_Claimed (
      Scheduler : in out Scheduler_Type;
      Residual  :        Quota_Type);

   --
   --  Next_Fill
   --
   procedure Next_Fill (Scheduler : in out Scheduler_Type);

   --
   --  Consumed
   --
   procedure Consumed (
      Scheduler : in out Scheduler_Type;
      Quota     :        Quota_Type);

   --
   --  Next_Round
   --
   procedure Next_Round (Scheduler : in out Scheduler_Type);

   --
   --  Reset_Claims
   --
   procedure Reset_Claims (
      Scheduler : in out Scheduler_Type;
      Prio      :        Priority_Type);

   --
   --  Share_Reset
   --
   procedure Share_Reset (Share : Share_Reference_Type);

end CPU_Scheduler;
