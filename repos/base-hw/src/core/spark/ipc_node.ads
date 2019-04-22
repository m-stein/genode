pragma Ada_2012;

package IPC_Node is

   type Byte_Type             is range 0 .. 2**8 - 1 with Size => 8;
   type Object_Size_Type      is range 0 .. 2**32 - 1 with Size => 32;
   type Thread_Type           is array (1 .. 32) of Byte_Type;
   type Thread_Reference_Type is not null access Thread_Type;
   type Object_Type           is private;
   type Object_Reference_Type is not null access Thread_Type;

   function Object_Size (Obj : Object_Type) return Object_Size_Type;

   procedure Initialize_Object (
      Obj  : out Object_Type;
      Thrd :     Thread_Reference_Type);

private

   type State_Type          is (Inactive, Wait_For_Reply, Wait_For_Request);
   type Object_Pointer_Type is access Object_Type;

   type Object_Type is record
      Thread : Thread_Reference_Type;
      State  : State_Type;
      Caller : Object_Pointer_Type;
      Callee : Object_Pointer_Type;
      Help   : Boolean;
   end record;

end IPC_Node;
