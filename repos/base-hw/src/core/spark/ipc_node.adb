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

end IPC_Node;
