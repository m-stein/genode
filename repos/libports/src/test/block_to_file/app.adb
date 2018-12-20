pragma Ada_2012;

with System;

package body App with Spark_Mode is


	procedure Log(Message : String) with Spark_Mode => Off is
		pragma warnings(off, "no global contract");

		-- append terminating null to string
		C_Message : String := Message & Character'Val(0);

		procedure C_Genode_Log(C_Message : System.Address) with
			import, convention => c, external_name => "c_genode_log";

	begin
		-- call C backend with null-terminated version of the string
		C_Genode_Log(C_Message'Address);
	end Log;


	procedure Error(Message : String) with Spark_Mode => Off is
		pragma warnings(off, "no global contract");

		-- append terminating null to string
		C_Message : String := Message & Character'Val(0);

		procedure C_Genode_Error(C_Message : System.Address) with
			import, convention => c, external_name => "c_genode_error";

	begin
		-- call C backend with null-terminated version of the string
		C_Genode_Error(C_Message'Address);
	end Error;


end App;
