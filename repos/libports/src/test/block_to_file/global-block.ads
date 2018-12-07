pragma Ada_2012;

package Global.Block
with Spark_Mode
is

   Max_Size : constant Sector_Type := 1024;

   procedure Read (Offset       : in Offset_Type;
                   Size         : in Size_Type;
                   Operation    : in Operation_Type;
                   Block_Number : in Sector_Type;
                   Block_Count  : in Size_Type;
                   Success      : in Natural)
     with
       export,
       convention => c,
       external_name => "ada_block_read";

private

   Procedure Internal_Read (Block_Number : in Sector_Type;
                            Block_Count  : in Size_Type)
     with
       pre => Block_Count > 0
       and Block_Number + Sector_Type(Block_Count) < Max_Size;

end Global.Block;
