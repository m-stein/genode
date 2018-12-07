pragma Ada_2012;

package body Global.Pending_Packets is

   procedure Insert (Packet : in Packet_Type) is
   begin
      Packet_Array(1) := Packet;
   end Insert;

end Global.Pending_Packets;
