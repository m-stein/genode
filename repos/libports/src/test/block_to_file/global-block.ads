pragma Ada_2012;

with Global.Packet_Buffer;

package Global.Block with Spark_Mode is

   type Object_Type is record
      Packet_Buffer_Object : Packet_Buffer.Object_Type;
   end record;

   Object     : Object_Type;
   Max_Blocks : constant Sector_Type := 1024;

   procedure Read (Offset       : Offset_Type;
                   Size         : Size_Type;
                   Operation    : Operation_Type;
                   Block_Number : Sector_Type;
                   Block_Count  : Size_Type;
                   Success      : Natural)
     with
       export, convention => c, external_name => "ada_block_read";

private

   Procedure Internal_Read (Object : in out Object_Type;
                            Packet : in     Packet_Type) with
     pre =>
       Packet.Block_Count > 0
       and not Packet_Buffer.Full(Object.Packet_Buffer_Object)
       and Packet.Block_Number + Sector_Type(Packet.Block_Count) < Max_Blocks;

end Global.Block;
