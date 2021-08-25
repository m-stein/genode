--
--  \brief  Control mutual exclusion of multiple CPUs on shared kernel data
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

with CPP; use CPP;

package Lock_Pkg is

   type Lock_Type is private;

   --
   --  Initialize a new lock object
   --
   procedure Initialize (
      Lock : out Lock_Type);

   --
   --  Acquire a lock for the executing CPU
   --
   procedure Lock (
      Lock : in out Lock_Type);

   --
   --  Release a lock, so, it can be acquired again
   --
   procedure Unlock (
      Lock : in out Lock_Type);

private

   type State_Type is (Unlocked, Locked) with Size => 32;

   type Lock_Type is record
      State        : State_Type;
      CPU_ID_Valid : Boolean;
      CPU_ID       : CPU_ID_Type;
   end record;

end Lock_Pkg;
