pragma Ada_2012;

package App with Spark_Mode is

	pragma Pure;

	type Byte           is mod 2**8;
	type Uint32         is mod 2**32;
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

--struct Block::Request
--{
--	enum class Operation : Genode::uint32_t { INVALID, READ, WRITE, SYNC };
--	enum class Success   : Genode::uint32_t { FALSE, TRUE };
--
--	Operation         operation;
--	Success           success;
--	Genode::uint64_t  offset;
--	Genode::uint32_t  size;
--
--	bool operation_defined() const
--	{
--		return operation == Operation::READ
--		    || operation == Operation::WRITE
--		    || operation == Operation::SYNC;
--	}
--
--} __attribute__((packed));

	type C_Boolean        is (C_False, C_True)            with Size => 8;
	type C_Operation_Type is (Invalid, Read, Write, Sync) with Size => 32;
	type C_Success_Type   is new C_Boolean                with Size => 32;
	type C_Offset_Type    is new Integer                    with Size => 64;
	type C_Size_Type      is mod 2**32                    with Size => 32;

	type C_Request_Type is record
		Operation : C_Operation_Type;
		Success   : C_Success_Type;
		Offset    : C_Offset_Type;
		Size      : C_Size_Type;
	end record;

	function To_C_Boolean(Value : in Boolean) return C_Boolean;

	---------------------------------------
	-- Interface to Genode LOG functions --
	---------------------------------------

	procedure Log_Unsigned_Long(Message : Natural) with
		import, convention => c, external_name => "c_genode_log_unsigned_long";

	procedure Log(Message : String);
	procedure Error(Message : String);

end App;
