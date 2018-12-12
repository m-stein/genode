pragma Ada_2012;

package body Global.Block with Spark_Mode is
   
   
   procedure Read (Offset       : Offset_Type;
                   Size         : Size_Type;
                   Operation    : Operation_Type;
                   Block_Number : Sector_Type;
                   Block_Count  : Size_Type;
                   Success      : Natural)
   is
   begin
      if Block_Number + Sector_Type(Block_Count) >= Max_Blocks then
         Error("attempt to read invalid block number");
      elsif Block_Count = 0 then
         Error("block count of read request is zero");
      elsif Packet_Buffer.Full(Object.Packet_Buffer_Object) then
         Error("packet buffer full");   
      else 
         Internal_Read(Object,
                       Packet_Type'(Offset       => Offset,
                                    Size         => Size,
                                    Operation    => Operation,
                                    Block_Number => Block_Number,
                                    Block_Count  => Block_Count,
                                    Success      => Success /= 0
                                   )
                      );
      end if;
   end Read;

   procedure Internal_Read (Object : in out Object_Type;
                            Packet : in     Packet_Type)
   is
   begin
      Log("Ada Read");
      Packet_Buffer.Insert(Object.Packet_Buffer_Object, Packet);
   end Internal_Read;

end Global.Block;
