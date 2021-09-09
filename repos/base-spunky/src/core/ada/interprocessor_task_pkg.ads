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

with CPP_Thread;
with Generic_Double_List;

with CPP;              use CPP;
with CPP_Architecture; use CPP_Architecture;

package Interprocessor_Task_Pkg is

   type Task_Type is private;
   type Task_Reference_Type is not null access all Task_Type;

   package Task_List_Pkg is new Generic_Double_List (
      Task_Type,
      Task_Reference_Type);

   procedure Task_List_Execute_Each (
      Task_List : Task_List_Pkg.List_Type);

   procedure Initialize_Thread_Destruction (
      Tsk                : Task_Reference_Type;
      Remote_Task_List   : Task_List_Pkg.List_Reference_Type;
      Caller             : CPP_Thread.Object_Reference_Type;
      Thread_To_Destruct : Address_Type);

   procedure Initialize_TLB_Invalidation (
      Tsk              : Task_Reference_Type;
      Global_Task_List : Task_List_Pkg.List_Reference_Type;
      Caller           : CPP_Thread.Object_Reference_Type;
      Nr_Of_CPUs       : Number_Of_CPUs_Type);

   procedure Execute (Tsk : Task_Reference_Type);

private

   type Operation_Type is (Thread_Destruction, TLB_Invalidation);

   type Task_Type is record
      List_Item          : aliased Task_List_Pkg.Item_Type;
      List               :         Task_List_Pkg.List_Reference_Type;
      Operation          :         Operation_Type;
      Caller             :         CPP_Thread.Object_Reference_Type;
      Thread_To_Destruct :         Address_Type;
      Nr_Of_Pending_CPUs :         Number_Of_CPUs_Type;
   end record;

end Interprocessor_Task_Pkg;
