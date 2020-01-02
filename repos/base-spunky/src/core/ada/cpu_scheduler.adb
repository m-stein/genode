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

package body CPU_Scheduler
is
   --
   --  Share_Initialize
   --
   procedure Share_Initialize (
      Share : Share_Reference_Type;
      Prio  : Priority_Type;
      Quota : Quota_Type)
   is
   begin
      Share_List.Item_Initialize (Share.Fill_Item, Share);
      Share_List.Item_Initialize (Share.Claim_Item, Share);
      Share.Prio := Prio;
      Share.Quota := Quota;
      Share.Claim := Quota;
      Share.Fill := 0;
      Share.Ready := False;
   end Share_Initialize;

   --
   --  Share_Ready
   --
   function Share_Ready (Share : Share_Type)
   return Boolean
   is (Share.Ready);

   --
   --  Share_Quota
   --
   procedure Share_Quota (
      Share : in out Share_Type;
      Quota :        Quota_Type)
   is
   begin
      Share.Quota := Quota;
   end Share_Quota;

   --
   --  Initialize
   --
   procedure Initialize (
      Scheduler : out Scheduler_Type;
      Idle      :     Share_Reference_Type;
      Quota     :     Quota_Type;
      Fill      :     Quota_Type)
   is
   begin
      for Prio in reverse Scheduler.Claims_Ready'Range loop
         Share_List.Initialize (Scheduler.Claims_Ready (Prio));
         Share_List.Initialize (Scheduler.Claims_Unready (Prio));
      end loop;
      Share_List.Initialize (Scheduler.Fills);
      Scheduler.Idle := Idle;
      Scheduler.Head := null;
      Scheduler.Head_Quota := 0;
      Scheduler.Head_Claims := False;
      Scheduler.Head_Yields := False;
      Scheduler.Quota := Quota;
      Scheduler.Residual := Quota;
      Scheduler.Fill := Fill;
      Scheduler.Need_To_Schedule := True;
      Scheduler.Last_Time := 0;
      Set_Head (Scheduler, Idle, Fill, False);
   end Initialize;

   --
   --  Head
   --
   function Head (Scheduler : Scheduler_Type)
   return Share_Reference_Type
   is (Share_Reference_Type (Scheduler.Head));

   --
   --  Head_Quota
   --
   function Head_Quota (Scheduler : Scheduler_Type)
   return Quota_Type
   is (Scheduler.Head_Quota);

   --
   --  Need_To_Schedule
   --
   function Need_To_Schedule (Scheduler : Scheduler_Type)
   return Boolean
   is (Scheduler.Need_To_Schedule);

   --
   --  Timeout
   --
   procedure Timeout (Scheduler : in out Scheduler_Type)
   is
   begin
      Scheduler.Need_To_Schedule := True;
   end Timeout;

   --
   --  Quota
   --
   procedure Quota (
      Scheduler : in out Scheduler_Type;
      Share     :        Share_Reference_Type;
      Quota     :        Quota_Type)
   is
   begin
      if Share = Scheduler.Idle then
         raise Program_Error;
      end if;
      if Share.Quota > 0 then
         Quota_Adaption (Scheduler, Share, Quota);
      elsif Quota > 0 then
         Quota_Introduction (Scheduler, Share);
      end if;
      Share.Quota := Quota;
   end Quota;

   --
   --  Insert
   --
   procedure Insert (
      Scheduler : in out Scheduler_Type;
      Share     :        Share_Reference_Type)
   is
   begin
      if Share.Ready then
         raise Program_Error;
      end if;
      Scheduler.Need_To_Schedule := True;
      if Share.Quota = 0 then
         return;
      end if;
      Share.Claim := Share.Quota;
      Share_List.Insert_Head (
         Scheduler.Claims_Unready (Share.Prio), Share.Claim_Item'Access);
   end Insert;

   --
   --  Remove
   --
   procedure Remove (
      Scheduler : in out Scheduler_Type;
      Share     :        Share_Reference_Type)
   is
   begin
      if Share = Scheduler.Idle then
         raise Program_Error;
      end if;
      Scheduler.Need_To_Schedule := True;
      if Scheduler.Head = Share_Pointer_Type (Share) then
         Scheduler.Head := null;
      end if;
      if Share.Ready then
         Share_List.Remove (Scheduler.Fills, Share.Fill_Item'Access);
      end if;
      if Share.Quota = 0 then
         return;
      end if;
      if Share.Ready then
         Share_List.Remove (
            Scheduler.Claims_Ready (Share.Prio), Share.Claim_Item'Access);
      else
         Share_List.Remove (
            Scheduler.Claims_Unready (Share.Prio), Share.Claim_Item'Access);
      end if;
   end Remove;

   --
   --  Yield
   --
   procedure Yield (Scheduler : in out Scheduler_Type)
   is
   begin
      Scheduler.Head_Yields := True;
      Scheduler.Need_To_Schedule := True;
   end Yield;

   --
   --  Unready
   --
   procedure Unready (
      Scheduler : in out Scheduler_Type;
      Share     :        Share_Reference_Type)
   is
   begin
      if not Share.Ready or else
         Share = Scheduler.Idle
      then
         raise Program_Error;
      end if;
      Scheduler.Need_To_Schedule := True;
      Share.Ready := False;
      Share_List.Remove (Scheduler.Fills, Share.Fill_Item'Access);
      if Share.Quota = 0 then
         return;
      end if;
      Share_List.Remove (
         Scheduler.Claims_Ready (Share.Prio), Share.Claim_Item'Access);

      Share_List.Insert_Tail (
         Scheduler.Claims_Unready (Share.Prio), Share.Claim_Item'Access);

   end Unready;

   --
   --  Ready
   --
   procedure Ready (
      Scheduler : in out Scheduler_Type;
      Share     :        Share_Reference_Type)
   is
   begin
      if Share.Ready or else
         Share = Scheduler.Idle
      then
         raise Program_Error;
      end if;
      Scheduler.Need_To_Schedule := True;
      Share.Ready := True;
      Share.Fill := Scheduler.Fill;
      Share_List.Insert_Tail (Scheduler.Fills, Share.Fill_Item'Access);
      if Share.Quota = 0 then
         return;
      end if;
      Share_List.Remove (
         Scheduler.Claims_Unready (Share.Prio), Share.Claim_Item'Access);

      if Share.Claim /= 0 then
         Share_List.Insert_Head (
            Scheduler.Claims_Ready (Share.Prio), Share.Claim_Item'Access);

      else
         Share_List.Insert_Tail (
            Scheduler.Claims_Ready (Share.Prio), Share.Claim_Item'Access);

      end if;
   end Ready;

   --
   --  Ready_Check
   --
   procedure Ready_Check (
      Scheduler : in out Scheduler_Type;
      Share_1   :        Share_Reference_Type)
   is
   begin
      if Scheduler.Head = null then
         raise Program_Error;
      end if;
      Ready (Scheduler, Share_1);
      if Scheduler.Need_To_Schedule then
         return;
      end if;
      declare
         Share_2 : Share_Pointer_Type := Scheduler.Head;
      begin
         if Share_1.Claim = 0 then
            if Share_2 = Share_Pointer_Type (Scheduler.Idle) then
               Scheduler.Need_To_Schedule := True;
            else
               Scheduler.Need_To_Schedule := False;
            end if;
         elsif not Scheduler.Head_Claims then
            Scheduler.Need_To_Schedule := True;
         elsif Share_1.Prio /= Share_2.Prio then
            if Share_1.Prio > Share_2.Prio then
               Scheduler.Need_To_Schedule := True;
            else
               Scheduler.Need_To_Schedule := False;
            end if;
         else
            while
               Share_2 /= null and then
               Share_2 /= Share_Pointer_Type (Share_1)
            loop
               if Share_List."/=" (
                  Share_List.Item_Next (Share_2.Claim_Item'Access), null)
               then
                  Share_2 :=
                     Share_Pointer_Type (
                        Share_List.Item_Payload (
                           Share_List.Item_Reference_Type (
                              Share_List.Item_Next (
                                 Share_2.Claim_Item'Access))));

               else
                  Share_2 := null;
               end if;
            end loop;
            if Share_2 = null then
               Scheduler.Need_To_Schedule := True;
            else
               Scheduler.Need_To_Schedule := False;
            end if;
         end if;
      end;
   end Ready_Check;

   --
   --  Update
   --
   procedure Update (
      Scheduler : in out Scheduler_Type;
      Time      :        Time_Type)
   is
      Quota : Quota_Type := Quota_Type (Time - Scheduler.Last_Time);
   begin
      Scheduler.Last_Time := Time;
      Scheduler.Need_To_Schedule := False;
      if Scheduler.Head /= null then
         declare
            Residual : Quota_Type;
         begin
            Trim_Consumption (Scheduler, Quota, Residual);
            if Scheduler.Head_Claims then
               Head_Claimed (Scheduler, Residual);
            else
               Head_Filled (Scheduler, Residual);
            end if;
         end;
         Consumed (Scheduler, Quota);
      end if;
      declare
         Found_Claim : Boolean;
      begin
         Claim_For_Head (Scheduler, Found_Claim);
         if Found_Claim then
            return;
         end if;
      end;
      declare
         Found_Fill : Boolean;
      begin
         Fill_For_Head (Scheduler, Found_Fill);
         if Found_Fill then
            return;
         end if;
      end;
      Set_Head (Scheduler, Scheduler.Idle, Scheduler.Fill, False);
   end Update;

   --
   --  Quota_Adaption
   --
   procedure Quota_Adaption (
      Scheduler : in out Scheduler_Type;
      Share     :        Share_Reference_Type;
      Quota     :        Quota_Type)
   is
   begin
      if Quota > 0 then
         if Share.Claim > Quota then
            Share.Claim := Quota;
         end if;
      else
         if Share.Ready then
            Share_List.Remove (
               Scheduler.Claims_Ready (Share.Prio), Share.Claim_Item'Access);
         else
            Share_List.Remove (
               Scheduler.Claims_Unready (Share.Prio), Share.Claim_Item'Access);
         end if;
      end if;
   end Quota_Adaption;

   --
   --  Quota_Introduction
   --
   procedure Quota_Introduction (
      Scheduler : in out Scheduler_Type;
      Share     :        Share_Reference_Type)
   is
   begin
      if Share.Ready then
         Share_List.Insert_Tail (
            Scheduler.Claims_Ready (Share.Prio), Share.Claim_Item'Access);
      else
         Share_List.Insert_Tail (
            Scheduler.Claims_Unready (Share.Prio), Share.Claim_Item'Access);
      end if;
   end Quota_Introduction;

   --
   --  Set_Head
   --
   procedure Set_Head (
      Scheduler : in out Scheduler_Type;
      Share     :        Share_Reference_Type;
      Quota     :        Quota_Type;
      Claims    :        Boolean)
   is
   begin
      Scheduler.Head_Quota := Quota;
      Scheduler.Head_Claims := Claims;
      Scheduler.Head := Share_Pointer_Type (Share);
   end Set_Head;

   --
   --  Trim_Consumption
   --
   procedure Trim_Consumption (
      Scheduler : in out Scheduler_Type;
      Quota     : in out Quota_Type;
      Residual  :    out Quota_Type)
   is
   begin
      if Quota > Scheduler.Head_Quota then
         Quota := Scheduler.Head_Quota;
      end if;
      if Quota > Scheduler.Residual then
         Quota := Scheduler.Residual;
      end if;
      if not Scheduler.Head_Yields then
         Residual := Scheduler.Head_Quota - Quota;
         return;
      end if;
      Scheduler.Head_Yields := False;
      Residual := 0;
   end Trim_Consumption;

   --
   --  Fill_For_Head
   --
   procedure Fill_For_Head (
      Scheduler  : in out Scheduler_Type;
      Fill_Found :    out Boolean)
   is
   begin
      if Share_List."=" (Share_List.Head (Scheduler.Fills), null) then
         Fill_Found := False;
         return;
      end if;
      declare
         Share : constant Share_Reference_Type :=
            Share_List.Item_Payload (
               Share_List.Item_Reference_Type (
                  Share_List.Head (Scheduler.Fills)));
      begin
         Set_Head (Scheduler, Share, Share.Fill, False);
      end;
      Fill_Found := True;
   end Fill_For_Head;

   --
   --  Claim_For_Head
   --
   procedure Claim_For_Head (
      Scheduler   : in out Scheduler_Type;
      Claim_Found :    out Boolean)
   is
   begin
      for Prio in reverse Scheduler.Claims_Ready'Range loop
         if Share_List."/=" (
            Share_List.Head (Scheduler.Claims_Ready (Prio)), null)
         then
            declare
               Share : constant Share_Reference_Type :=
                  Share_List.Item_Payload (
                     Share_List.Item_Reference_Type (
                        Share_List.Head (Scheduler.Claims_Ready (Prio))));
            begin
               if Share.Claim > 0 then
                  Set_Head (Scheduler, Share, Share.Claim, True);
                  Claim_Found := True;
                  return;
               end if;
            end;
         end if;
      end loop;
      Claim_Found := False;
   end Claim_For_Head;

   --
   --  Head_Filled
   --
   procedure Head_Filled (
      Scheduler : in out Scheduler_Type;
      Residual  :        Quota_Type)
   is
   begin
      if Share_List."/=" (
         Share_List.Head (Scheduler.Fills), Scheduler.Head.Fill_Item'Access)
      then
         return;
      end if;
      if Residual > 0 then
         Scheduler.Head.Fill := Residual;
      else
         Next_Fill (Scheduler);
      end if;
   end Head_Filled;

   --
   --  Head_Claimed
   --
   procedure Head_Claimed (
      Scheduler : in out Scheduler_Type;
      Residual  :        Quota_Type)
   is
   begin
      if Scheduler.Head.Quota = 0 then
         return;
      end if;
      if Residual > Scheduler.Head.Quota then
         Scheduler.Head.Claim := Scheduler.Head.Quota;
      else
         Scheduler.Head.Claim := Residual;
      end if;
      if Scheduler.Head.Claim > 0 or else
         not Scheduler.Head.Ready
      then
         return;
      end if;
      Share_List.To_Tail (
         Scheduler.Claims_Ready (Scheduler.Head.Prio),
         Scheduler.Head.Claim_Item'Access);
   end Head_Claimed;

   --
   --  Next_Fill
   --
   procedure Next_Fill (Scheduler : in out Scheduler_Type)
   is
   begin
      Scheduler.Head.Fill := Scheduler.Fill;
      Share_List.Head_To_Tail (Scheduler.Fills);
   end Next_Fill;

   --
   --  Consumed
   --
   procedure Consumed (
      Scheduler : in out Scheduler_Type;
      Quota     :        Quota_Type)
   is
   begin
      if Scheduler.Residual > Quota then
         Scheduler.Residual := Scheduler.Residual - Quota;
      else
         Next_Round (Scheduler);
      end if;
   end Consumed;

   --
   --  Next_Round
   --
   procedure Next_Round (Scheduler : in out Scheduler_Type)
   is
   begin
      Scheduler.Residual := Scheduler.Quota;
      for Prio in reverse Scheduler.Claims_Ready'Range loop
         Reset_Claims (Scheduler, Prio);
      end loop;
   end Next_Round;

   --
   --  Reset_Claims
   --
   procedure Reset_Claims (
      Scheduler : in out Scheduler_Type;
      Prio      :        Priority_Type)
   is
   begin
      Share_List.For_Each (Scheduler.Claims_Ready (Prio), Share_Reset'Access);
      Share_List.For_Each (
         Scheduler.Claims_Unready (Prio), Share_Reset'Access);

   end Reset_Claims;

   --
   --  Share_Reset
   --
   procedure Share_Reset (Share : Share_Reference_Type)
   is
   begin
      Share.Claim := Share.Quota;
   end Share_Reset;

end CPU_Scheduler;
