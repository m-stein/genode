--
--  \brief  Utility for accessing the CPUs model-specific registers (MSR)
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

with CPP_Architecture; use CPP_Architecture;

generic
   MSR_Address : MSR_Address_Type;
   type MSR_Type is private;
package CPU_Model_Specific_Register_Pkg is

   function Read
   return MSR_Type;

   procedure Write (
      MSR : MSR_Type);

end CPU_Model_Specific_Register_Pkg;
