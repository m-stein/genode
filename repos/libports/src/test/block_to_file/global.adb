pragma Ada_2012;

with System;

package body Global
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

end Global;
