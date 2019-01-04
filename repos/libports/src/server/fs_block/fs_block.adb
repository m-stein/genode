pragma Ada_2012;

package body Fs_Block with Spark_Mode is


	function Correct_Object_Size(Object        : in Object_Type;
	                             Size_In_Bytes : in Natural) return Boolean
	is
		(Size_In_Bytes /= Bits_To_Bytes(Object'Size));


	procedure Initialize_Object(Object        : out Object_Type;
	                            Size_In_Bytes : in  Natural)
	is begin

		-- check that size of C++ type and Ada type are the same
		if not Correct_Object_Size(Object, Size_In_Bytes) then
			Error("size mismatch when initializing object");
--			Log_Unsigned_Long(C_Size_In_Bytes);
--			Log_Unsigned_Long(Bits_To_Bytes(Object'Size));
			raise Program_Error;
		end if;

		-- initialize object members
		Request_Buffer.Initialize_Object(Object.Request_Buffer_Object);

	end Initialize_Object;


	function Acceptable(Object : in Object_Type) return C_Boolean is
		 (To_C_Boolean(not Request_Buffer.Full(Object.Request_Buffer_Object)));


	function Request_In_Buffer(Object  : in Object_Type;
	                           Request : in Request_Type) return Boolean
	is
		(Request_Buffer.Request_In_Buffer(Object.Request_Buffer_Object,
		                                  Request));


	function Request_In_Device_Range(Request : in Request_Type) return Boolean is
		(Request.Offset + C_Offset_Type(Request.Size)
			< C_Offset_Type(Max_Device_Size));


	procedure Submit(Object  : in out Object_Type;
	                 Request : in     Request_Type)
	is begin

--		Log("Submit");
--		Log_Uint64(Uint64(Request.Offset));
--		Log_Unsigned_Long(Integer(Request.Size));
		Request_Buffer.Insert(Object.Request_Buffer_Object, Request);

	end Submit;


end Fs_Block;
