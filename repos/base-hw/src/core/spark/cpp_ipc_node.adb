pragma Ada_2012;

package body CPP_IPC_Node is

   procedure Initialize_Object (
      Obj  : IPC_Node.Object_Reference_Type;
      Thrd : IPC_Node.Thread_Reference_Type)
   is
   begin
      IPC_Node.Initialize_Object (Obj, Thrd);
   end Initialize_Object;

   function Thread (Obj : IPC_Node.Object_Reference_Type)
   return IPC_Node.Thread_Reference_Type
   is (IPC_Node.Thread (Obj));

end CPP_IPC_Node;
