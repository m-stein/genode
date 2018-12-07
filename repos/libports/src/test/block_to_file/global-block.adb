pragma Ada_2012;

with Global.Pending_Packets;

package body Global.Block
with Spark_Mode => On
is
   procedure Read (Offset       : in Offset_Type;
                   Size         : in Size_Type;
                   Operation    : in Operation_Type;
                   Block_Number : in Sector_Type;
                   Block_Count  : in Size_Type;
                   Success      : in Natural)
   is
   begin
      if Block_Number + Sector_Type(Block_Count) >= Max_Size then
         Log("Error: Bad Access Offset");
      else
         if Block_Count = 0 then
            Log("Error: Bad Block_Count ");
         else
            Pending_Packets.Insert(Packet_Type'(Offset       => Offset,
                                                Size         => Size,
                                                Operation    => Operation,
                                                Block_Number => Block_Number,
                                                Block_Count  => Block_Count,
                                                Success      => Success /= 0
                                               )
                                  );
            Internal_Read(Block_Number, Block_Count);
         end if;
      end if;
   end Read;

   procedure Internal_Read (Block_Number : in Sector_Type;
                            Block_Count  : in Size_Type)
   is
      pragma warnings(off, "no global contract");
      procedure ext_c_print_sector(a : Sector_Type)
        with
          import,
          convention => c,
          external_name => "print_sector";

      procedure ext_c_print_size(a : Size_Type)
        with
          import,
          convention => c,
          external_name => "print_size";
   begin
      Log("Ada Read");
      ext_c_print_sector(Block_Number);
      ext_c_print_size(Block_Count);
   end Internal_Read;


end Global.Block;
