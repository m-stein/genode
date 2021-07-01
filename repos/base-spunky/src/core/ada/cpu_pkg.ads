--
--  \brief  Compound of all per-CPU objects in the kernel
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

with CPU_Device_Pkg;
with Timer_Pkg;
with IRQ_Controller_Pkg;
with CPU_Scheduler;

with CPP;              use CPP;
with CPP_Architecture; use CPP_Architecture;

package CPU_Pkg is

   type CPU_Type is private;

   function CPU_Size (CPU : CPU_Type)
   return Size_Type
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZNK6Kernel18Imitating_ada_typeINS_3CpuEE9_ada_sizeEv";

private

   type Placeholder_1_Type is array (1 .. 40) of Byte_Type with Pack;
   type Placeholder_2_Type is array (1 .. 1400) of Byte_Type with Pack;

   type CPU_Type is record
      CPU_Device             : CPU_Device_Pkg.CPU_Device_Type;
      Placeholder_1          : Placeholder_1_Type;
      Scheduling_Timeout     : Timer_Pkg.Timeout_Type;
      ID                     : CPU_ID_Type;
      IRQ_Controller         : IRQ_Controller_Pkg.IRQ_Controller_Type;
      Timer                  : Timer_Pkg.Timer_Type;
      Scheduler              : CPU_Scheduler.Scheduler_Type;
      Placeholder_2          : Placeholder_2_Type;
   end record;

end CPU_Pkg;
