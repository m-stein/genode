--
--  \brief  Peer of message-based synchronous inter-process communication
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

package body IPC_Node is

   package body Queue is

      package body Item is

         function Initialized_Object (Payload : Object_Reference_Type)
         return Ibject_Type
         is (
            Next    => null,
            Payload => Payload);

      end Item;

      function Initialized_Object return Qbject_Type
      is (
         Head => null,
         Tail => null);

   end Queue;

   procedure Initialize_Object (
      Obj  : Object_Reference_Type;
      Thrd : Thread_Reference_Type)
   is
   begin
      Obj.all := (
         Thread             => Thrd,
         State              => Inactive,
         Caller             => null,
         Callee             => null,
         Help               => False,
         Request_Queue      => Queue.Initialized_Object,
         Request_Queue_Item => Queue.Item.Initialized_Object (Obj));

   end Initialize_Object;

   function Can_Send_Request (Obj : Object_Reference_Type)
   return Boolean
   is (Obj.State = Inactive);

   function Thread (Obj : Object_Reference_Type)
   return Thread_Reference_Type
   is (Obj.Thread);

end IPC_Node;
