pragma Ada_2012;

package Utils with Spark_Mode is

	pragma Pure;

	type Byte           is mod 2**8;
	type Uint32         is mod 2**32;
	type Uint64         is mod 2**64;
	type Sector_Type    is mod 2**64;
	type Size_Type      is mod 2**64;
	type Offset_Type    is new Integer;
	type Operation_Type is (Read, Write, Stop);

	type C_Boolean        is (C_False, C_True)            with Size => 1 * 8;
	type C_Operation_Type is (Invalid, Read, Write, Sync) with Size => 4 * 8;
	type C_Success_Type   is new C_Boolean                with Size => 4 * 8;
	type C_Offset_Type    is new Uint64                   with Size => 8 * 8;
	type C_Size_Type      is new Uint32                   with Size => 4 * 8;

	type Request_Type is record

		Operation : C_Operation_Type;
		Success   : C_Success_Type;
		Offset    : C_Offset_Type;
		Size      : C_Size_Type;

	end record; pragma Pack(Request_Type);

	function To_C_Boolean(Value : in Boolean) return C_Boolean is
		(if Value then C_True else C_False);

	function To_Spark_Boolean(Value : in C_Boolean) return Boolean is
		(if Value = C_True then True else False);

	function Bits_To_Bytes(Bits : in Natural) return Natural is
		(if Bits mod Byte'Size = 0 then  Bits / Byte'Size
		                           else (Bits / Byte'Size) + 1);

	function Request_Valid(Request : in Request_Type) return Boolean is
		(Request.Operation /= Invalid);

	---------------------------------------
	-- Interface to Genode LOG functions --
	---------------------------------------

	procedure Log_Uint64(Message : in Uint64) with
		import, convention => c, external_name => "c_genode_log_uint64";

	procedure Log_Unsigned_Long(Message : in Natural) with
		import, convention => c, external_name => "c_genode_log_unsigned_long";

	procedure Log(Message : in String);
	procedure Error(Message : String);

end Utils;
