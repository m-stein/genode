pragma Ada_2012;

package body IPC_Node is

   function Object_Size (Obj : Object_Type) return Object_Size_Type
   is (Obj'Size / 8);

   procedure Initialize_Object (Obj : out Object_Type)
   is
   begin
      Obj := (Help => False);
   end Initialize_Object;

end IPC_Node;
