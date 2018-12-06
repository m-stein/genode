with System;

package body Block
with Spark_Mode => On
is
   procedure Log(Message : String)
     with Spark_Mode => Off
   is
      pragma warnings(off, "no global contract");
      procedure Ext_C_Log(C_Message_Out : System.Address)
        with
          import,
          convention => c,
          external_name => "print_string";

      C_Message : String := Message & Character'Val(0);
   begin
      Ext_C_Log(C_Message'Address);
   end Log;

   procedure Read (Block_Number : in Sector;
                   Block_Count  : in Size)
   is
   begin
      if Block_Number + Sector(Block_Count) >= Max_Size then
         Log("Error: Bad Access Offset");
      else
         if Block_Count = 0 then
            Log("Error: Bad Block_Count ");
         else
            Internal_Read(Block_Number, Block_Count);
         end if;
      end if;
   end Read;

   procedure Internal_Read (Block_Number : in Sector;
                            Block_Count  : in Size)
   is
      pragma warnings(off, "no global contract");
      procedure ext_c_print_sector(a : Sector)
        with
          import,
          convention => c,
          external_name => "print_sector";

      procedure ext_c_print_size(a : Size)
        with
          import,
          convention => c,
          external_name => "print_size";
   begin
      Log("Ada Read");
      ext_c_print_sector(Block_Number);
      ext_c_print_size(Block_Count);
   end Internal_Read;


end Block;
