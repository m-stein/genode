pragma Ada_2012;

package Global
with Spark_Mode => On
is
   type Sector_Type    is mod 2**64;
   type Size_Type      is mod 2**64;
   type Offset_Type    is new Integer;
   type Operation_Type is (Read, Write, Stop);
   type Packet_Type is record
      Offset       : Offset_Type;
      Size         : Size_Type;
      Operation    : Operation_Type;
      Block_Number : Sector_Type;
      Block_Count  : Size_Type;
      Success      : Boolean;
   end record;

   procedure Log(Message : String);
   procedure Error(Message : String);

end Global;
