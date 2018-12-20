pragma Ada_2012;

package App with Spark_Mode is

	pragma Pure;

	type Byte           is mod 2**8;
	type Sector_Type    is mod 2**64;
	type Size_Type      is mod 2**64;
	type Offset_Type    is new Integer;
	type Operation_Type is (Read, Write, Stop);

	--
	-- Ada representation of Genode Packet_descriptor
	--
	type Packet_Type is record
		Offset       : Offset_Type;
		Size         : Size_Type;
		Operation    : Operation_Type;
		Block_Number : Sector_Type;
		Block_Count  : Size_Type;
		Success      : Boolean;
	end record;


	---------------------------------------
	-- Interface to Genode LOG functions --
	---------------------------------------

	procedure Log_Unsigned_Long(Message : Natural) with
		import, convention => c, external_name => "c_genode_log_unsigned_long";

	procedure Log(Message : String);
	procedure Error(Message : String);

end App;
