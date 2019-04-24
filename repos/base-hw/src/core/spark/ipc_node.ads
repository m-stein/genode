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

package IPC_Node is

   type Byte_Type             is range 0 .. 2**8 - 1 with Size => 8;
   type Thread_Type           is array (1 .. 32) of Byte_Type;
   type Thread_Reference_Type is not null access Thread_Type;
   type State_Type            is (Inactive, Wait_For_Reply, Wait_For_Request);

   type Object_Type           is private;
   type Object_Pointer_Type   is access Object_Type;
   type Object_Reference_Type is not null access Object_Type;

   package Queue is

      package Item is

         type Ibject_Type         is private;
         type Ibject_Pointer_Type is access Ibject_Type;

         function Initialized_Object (Payload : Object_Reference_Type)
         return Ibject_Type;

      private

         type Ibject_Type is record
            Next    : Ibject_Pointer_Type;
            Payload : Object_Reference_Type;
         end record;

      end Item;

      type Qbject_Type is private;

      function Initialized_Object return Qbject_Type;

   private

      type Qbject_Type is record
         Head : Item.Ibject_Pointer_Type;
         Tail : Item.Ibject_Pointer_Type;
      end record;

   end Queue;

   procedure Initialize_Object (
      Obj  : Object_Reference_Type;
      Thrd : Thread_Reference_Type);

   function Can_Send_Request (Obj : Object_Reference_Type)
   return Boolean;

   function Thread (Obj : Object_Reference_Type)
   return Thread_Reference_Type;

private

   type Object_Type is record
      Thread             : Thread_Reference_Type;
      State              : State_Type;
      Caller             : Object_Pointer_Type;
      Callee             : Object_Pointer_Type;
      Help               : Boolean;
      Request_Queue      : Queue.Qbject_Type;
      Request_Queue_Item : Queue.Item.Ibject_Type;
   end record;

end IPC_Node;
