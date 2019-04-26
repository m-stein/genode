--
--  \brief  Glue code between Ada and C++ interface of thread class
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

with CPP;

package CPP_Thread is

   type Object_Type           is private;
   type Object_Reference_Type is not null access Object_Type;

   procedure Print_Int (Obj : Integer)
   with
      Import,
      Convention    => C,
      External_Name => "print_int";

   procedure IPC_Wait_For_Request_Succeeded (Obj : Object_Reference_Type)
   with
      Import,
      Convention    => C,
      External_Name => "_ZN6Kernel6Thread27ipc_await_request_succeededEv";

   procedure IPC_Send_Request_Succeeded (Obj : Object_Reference_Type)
   with
      Import,
      Convention    => C,
      External_Name => "_ZN6Kernel6Thread26ipc_send_request_succeededEv";

   procedure IPC_Wait_For_Request_Failed (Obj : Object_Reference_Type)
   with
      Import,
      Convention    => C,
      External_Name => "_ZN6Kernel6Thread24ipc_await_request_failedEv";

   procedure IPC_Send_Request_Failed (Obj : Object_Reference_Type)
   with
      Import,
      Convention    => C,
      External_Name => "_ZN6Kernel6Thread23ipc_send_request_failedEv";

   procedure IPC_Copy_Message (
      Obj    : Object_Reference_Type;
      Sender : Object_Reference_Type)
   with
      Import,
      Convention    => C,
      External_Name => "_ZN6Kernel6Thread12ipc_copy_msgERS0_";

private

   type Object_Type is array (1 .. 32) of CPP.Byte_Type;

end CPP_Thread;
