--
--  \brief  Glue code between Ada and C++ interface of IPC node
--  \author Martin stein
--  \date   2019-04-24
--

--
--  Copyright (C) 2019 Genode Labs GmbH
--
--  This file is part of the Genode OS framework, which is distributed
--  under the terms of the GNU Affero General Public License version 3.
--

pragma Ada_2012;

package CPP_Thread is

   type Byte_Type             is range 0 .. 2**8 - 1 with Size => 8;
   type Object_Type           is array (1 .. 32) of Byte_Type;
   type Object_Reference_Type is not null access Object_Type;

   procedure IPC_Wait_For_Request_Succeeded (Obj : Object_Reference_Type)
   with
      Import,
      Convention    => C,
      External_Name => "_ZN6Kernel6Thread27ipc_await_request_succeededEv";

   procedure IPC_Copy_Message (
      Obj    : Object_Reference_Type;
      Sender : Object_Reference_Type)
   with
      Import,
      Convention    => C,
      External_Name => "_ZN6Kernel6Thread12ipc_copy_msgERS0_";

end CPP_Thread;
