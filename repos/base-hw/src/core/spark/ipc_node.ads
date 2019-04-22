pragma Ada_2012;

package IPC_Node is

   type Object_Size_Type is mod 2**32 with Size => 32;

   type Object_Type is private;

   function Object_Size (Obj : Object_Type) return Object_Size_Type;

   procedure Initialize_Object (Obj : out Object_Type);

private

   type Object_Type is record
      Help : Boolean;
   end record;

end IPC_Node;
