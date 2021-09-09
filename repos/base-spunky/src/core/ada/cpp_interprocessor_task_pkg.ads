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

with CPP_Thread;
with CPP;                     use CPP;
with CPP_Architecture;        use CPP_Architecture;
with Interprocessor_Task_Pkg; use Interprocessor_Task_Pkg;

package CPP_Interprocessor_Task_Pkg is

   function Task_Size (Tsk : Task_Type)
   return Size_Type
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZNK6Kernel15Opaque_ada_typeINS_20Inter_processor_workELm64EE9" &
         "_ada_sizeEv";

   function Task_List_Size (Task_List : Task_List_Pkg.List_Type)
   return Size_Type
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZNK6Kernel15Opaque_ada_typeINS_25" &
         "Inter_processor_work_listELm16EE9_ada_sizeEv";

   procedure Task_List_Initialize (
      Task_List : out Task_List_Pkg.List_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel25Inter_processor_work_listC1Ev";

   procedure Task_List_Execute_Each (
      Task_List : Task_List_Pkg.List_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN6Kernel25Inter_processor_work_list12execute_eachEv";

   procedure Initialize_Thread_Destruction (
      Tsk                : Task_Reference_Type;
      Remote_Task_List   : Task_List_Pkg.List_Reference_Type;
      Caller             : CPP_Thread.Object_Reference_Type;
      Thread_To_Destruct : Address_Type)
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN6Kernel20Inter_processor_workC2ERNS_25" &
         "Inter_processor_work_listERNS_6ThreadERN6Genode13" &
         "Kernel_objectIS3_EE";

   procedure Initialize_TLB_Invalidation (
      Tsk              : Task_Reference_Type;
      Global_Task_List : Task_List_Pkg.List_Reference_Type;
      Caller           : CPP_Thread.Object_Reference_Type;
      Nr_Of_CPUs       : Number_Of_CPUs_Type)
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN6Kernel20Inter_processor_workC2ERNS_25" &
         "Inter_processor_work_listERNS_6ThreadEj";

end CPP_Interprocessor_Task_Pkg;
