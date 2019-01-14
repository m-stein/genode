pragma Ada_2012;

package body Machinery1 is

   procedure Initialize (Machinery1 : out Machinery1_Type) is
   begin
      Machinery1 := ( Temperature => 25 );
   end Initialize;

   function Temperature (Machinery1 : Machinery1_Type) return Temperature_Type is
   begin
      return Machinery1.Temperature;
   end Temperature;

   procedure Heat_Up (Machinery1 : in out Machinery1_Type) is
   begin
      Machinery1.Temperature := 77;
   end Heat_Up;

end Machinery1;
