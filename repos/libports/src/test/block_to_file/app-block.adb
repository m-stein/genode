pragma Ada_2012;

package body App.Block with Spark_Mode is


	procedure Read(Object       : in out Object_Type;
	               Offset       : in     Offset_Type;
	               Size         : in     Size_Type;
	               Operation    : in     Operation_Type;
	               Block_Number : in     Sector_Type;
	               Block_Count  : in     Size_Type;
	               Success      : in     Natural)
	is begin

		-- fail if a precondition of the internal Read is not met
		if Block_Number + Sector_Type(Block_Count) >= Max_Blocks then
			Error("attempt to read invalid block number");
		elsif Block_Count = 0 then
			Error("block count of read request is zero");
		elsif Packet_Buffer.Full(Object.Packet_Buffer_Object) then
			Error("packet buffer full");
		else

			-- call internal Read whose preconditions are now true
			Internal_Read(Object,
			              Packet_Type'(Offset       => Offset,
			                           Size         => Size,
			                           Operation    => Operation,
			                           Block_Number => Block_Number,
			                           Block_Count  => Block_Count,
			                           Success      => Success /= 0));
		end if;

	end Read;


	function Acceptable(Object : in Object_Type) return C_Boolean is begin

		Log("Acceptable");
		if Packet_Buffer.Full(Object.Packet_Buffer_Object) then
			Log("Full");
		end if;
		return To_C_Boolean(not Packet_Buffer.Full(Object.Packet_Buffer_Object));

	end Acceptable;


	procedure Submit(Object  : in Object_Type;
	                 Request : in C_Request_Type)
	is begin
		Log("Submit");
		if Packet_Buffer.Full(Object.Packet_Buffer_Object) then
			Log("Full");
		end if;
		Log_Unsigned_Long(Integer(Request.Offset));
	end Submit;


	procedure Initialize_Object(Object : out Object_Type;
	                            Size   : in  Natural)
	is begin

		-- check that size of C++ type and Ada type are the same
		if Size /= Object'Size then
			Error("size mismatch when initializing object");
			Log_Unsigned_Long(Size);
			Log_Unsigned_Long(Object'Size);
			raise Program_Error;
		end if;

		-- initialize object members
		Packet_Buffer.Initialize_Object(Object.Packet_Buffer_Object);

	end Initialize_Object;


	procedure Internal_Read (Object : in out Object_Type;
	                         Packet : in     Packet_Type)
	is
--      Chunk_In_Slot : Boolean;
--      Slot_Index    : File_Cache.Slot_Array_Index_Type;
	begin
		Log("Ada Read");
		Packet_Buffer.Insert(Object.Packet_Buffer_Object,
		                     Packet);
--      File_Cache.Read_Chunk(Object.File_Cache_Object_Private,
--                            Object.File_Cache_Object_Public,
--                            0,
--                            Chunk_In_Slot,
--                            Slot_Index
--                           );
	end Internal_Read;


end App.Block;
