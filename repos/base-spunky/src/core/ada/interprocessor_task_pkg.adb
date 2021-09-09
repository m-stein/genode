--
--  \brief  Executing kernel tasks on remote CPUs
--  \author Martin stein
--  \date   2021-08-23
--

--
--  Copyright (C) 2021 Genode Labs GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

pragma Ada_2012;

with CPU_Device_Pkg;

package body Interprocessor_Task_Pkg is

   procedure Task_List_Execute_Each (
      Task_List : Task_List_Pkg.List_Type)
   is
   begin
      Task_List_Pkg.For_Each (Task_List, Execute'Access);
   end Task_List_Execute_Each;

   procedure Initialize_Thread_Destruction (
      Tsk                : Task_Reference_Type;
      Remote_Task_List   : Task_List_Pkg.List_Reference_Type;
      Caller             : CPP_Thread.Object_Reference_Type;
      Thread_To_Destruct : Address_Type)
   is
   begin

      Task_List_Pkg.Item_Initialize (Tsk.List_Item, Tsk);

      Tsk.Operation          := Thread_Destruction;
      Tsk.Nr_Of_Pending_CPUs := 1;
      Tsk.Caller             := Caller;
      Tsk.List               := Remote_Task_List;
      Tsk.Thread_To_Destruct := Thread_To_Destruct;

      Task_List_Pkg.Insert_Head_1 (Tsk.List, Tsk.List_Item'Access);

   end Initialize_Thread_Destruction;

   procedure Initialize_TLB_Invalidation (
      Tsk              : Task_Reference_Type;
      Global_Task_List : Task_List_Pkg.List_Reference_Type;
      Caller           : CPP_Thread.Object_Reference_Type;
      Nr_Of_CPUs       : Number_Of_CPUs_Type)
   is
   begin

      Task_List_Pkg.Item_Initialize (Tsk.List_Item, Tsk);

      Tsk.Operation          := TLB_Invalidation;
      Tsk.Nr_Of_Pending_CPUs := Nr_Of_CPUs;
      Tsk.Caller             := Caller;
      Tsk.List               := Global_Task_List;
      Tsk.Thread_To_Destruct := 0;

      Task_List_Pkg.Insert_Head_1 (Tsk.List, Tsk.List_Item'Access);

   end Initialize_TLB_Invalidation;

   procedure Execute (Tsk : Task_Reference_Type)
   is
   begin
      case Tsk.Operation is
      when Thread_Destruction =>
         CPP_Thread.Destruct (Tsk.Thread_To_Destruct);
      when TLB_Invalidation =>
         CPU_Device_Pkg.Invalidate_TLB;
      end case;
      Tsk.Nr_Of_Pending_CPUs := Tsk.Nr_Of_Pending_CPUs - 1;
      if Tsk.Nr_Of_Pending_CPUs = 0 then
         Task_List_Pkg.Remove_1 (Tsk.List, Tsk.List_Item'Access);
         CPP_Thread.Restart (Tsk.Caller);
      end if;
   end Execute;

end Interprocessor_Task_Pkg;
