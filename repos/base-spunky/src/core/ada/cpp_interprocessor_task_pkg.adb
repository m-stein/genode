--
--  \brief  Glue code between Ada and C++ interface of the lock package
--  \author Martin stein
--  \date   2021-06-09
--

--
--  Copyright (C) 2021 Genode Labs GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

pragma Ada_2012;

with Interfaces; use Interfaces;

package body CPP_Interprocessor_Task_Pkg is

   function Task_Size (Tsk : Task_Type)
   return Size_Type
   is (
      Tsk'Size / 8);

   function Task_List_Size (Task_List : Task_List_Pkg.List_Type)
   return Size_Type
   is (
      Task_List'Size / 8);

   procedure Task_List_Initialize (
      Task_List : out Task_List_Pkg.List_Type)
   is
   begin
      Task_List_Pkg.Initialize (Task_List);
   end Task_List_Initialize;

   procedure Task_List_Execute_Each (
      Task_List : Task_List_Pkg.List_Type)
   is
   begin
      Interprocessor_Task_Pkg.Task_List_Execute_Each (Task_List);
   end Task_List_Execute_Each;

   procedure Initialize_Thread_Destruction (
      Tsk                : Task_Reference_Type;
      Remote_Task_List   : Task_List_Pkg.List_Reference_Type;
      Caller             : CPP_Thread.Object_Reference_Type;
      Thread_To_Destruct : Address_Type)
   is
   begin
      Interprocessor_Task_Pkg.Initialize_Thread_Destruction (
         Tsk, Remote_Task_List, Caller, Thread_To_Destruct);
   end Initialize_Thread_Destruction;

   procedure Initialize_TLB_Invalidation (
      Tsk              : Task_Reference_Type;
      Global_Task_List : Task_List_Pkg.List_Reference_Type;
      Caller           : CPP_Thread.Object_Reference_Type;
      Nr_Of_CPUs       : Number_Of_CPUs_Type)
   is
   begin
      Interprocessor_Task_Pkg.Initialize_TLB_Invalidation (
         Tsk, Global_Task_List, Caller, Nr_Of_CPUs);
   end Initialize_TLB_Invalidation;

end CPP_Interprocessor_Task_Pkg;
