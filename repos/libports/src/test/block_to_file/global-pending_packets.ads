pragma Ada_2012;

package Global.Pending_Packets
with Spark_Mode => On
is
   type Packet_Array_Type is array (Positive range 1..1024) of Packet_Type;

   Packet_Array : Packet_Array_Type;

   procedure Insert(Packet : in Packet_Type);

end Global.Pending_Packets;
