package Block
with Spark_Mode
is
   type Sector is mod 2**64;
   type Size is mod 2**64;

   Max_Size : constant Sector := 1024;

   procedure Log(Message : String);

   procedure Read (Block_Number : in Sector;
                   Block_Count  : in Size)
     with
       export,
       convention => c,
       external_name => "ada_block_read";

private

   Procedure Internal_Read (Block_Number : in Sector;
                            Block_Count  : in Size)
     with
       pre => Block_Count > 0
       and Block_Number + Sector(Block_Count) < Max_Size;

end Block;
