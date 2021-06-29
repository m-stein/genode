--
--  \brief  Utility for accessing the CPUs control registers (CRx)
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

generic
   CR_Index : String;
   type CR_Type is private;
package CPU_Control_Register_Pkg is

   function Read
   return CR_Type;

   procedure Write (
      CR : CR_Type);

end CPU_Control_Register_Pkg;
