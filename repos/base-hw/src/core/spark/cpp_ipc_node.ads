pragma Ada_2012;

with IPC_Node;

package CPP_IPC_Node is

   procedure Initialize_Object (
      Obj  : IPC_Node.Object_Reference_Type;
      Thrd : IPC_Node.Thread_Reference_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN14Spark_ipc_nodeC1ERN6Kernel6ThreadE";

   function Thread (Obj : IPC_Node.Object_Reference_Type)
   return IPC_Node.Thread_Reference_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZN14Spark_ipc_node6threadEv";

end CPP_IPC_Node;
