pragma Ada_2012;

with App.Packet_Buffer;

package App.Block with Spark_Mode is

	pragma Pure;

	type Object_Type is private;

	-- maximum size of the block device as number of blocks
	Max_Blocks : constant Sector_Type := 1024;

	--
	-- Initialize an uninitialized package state
	--
	procedure Initialize_Object(Object : out Object_Type;
	                            Size   : in  Natural)
	with
		Export, Convention => C, External_Name => "_ZN5Spark15Block_processorC1Em";

	--
	-- Wether new requests can be processed with the given package state
	--
	function Acceptable(Object : in Object_Type) return C_Boolean
	with
		Export, Convention => C, External_Name => "_ZNK5Spark15Block_processor10acceptableEv",
		Depends => (Acceptable'Result => Object);

	--
	-- Wether new requests can be processed with the given package state
	--
	procedure Submit(Object  : in Object_Type;
	                 Request : in C_Request_Type)
	with
		Export, Convention => C, External_Name => "_ZN5Spark15Block_processor6submitEN5Block7RequestE";

	--
	-- Apply an incoming read request to a package state
	--
	procedure Read(Object       : in out Object_Type;
	               Offset       : in     Offset_Type;
	               Size         : in     Size_Type;
	               Operation    : in     Operation_Type;
	               Block_Number : in     Sector_Type;
	               Block_Count  : in     Size_Type;
	               Success      : in     Natural)
	with
	    Export, Convention => C, External_Name => "ada_block_read";

private

	type Object_Type is record
		Packet_Buffer_Object      : Packet_Buffer.Object_Type;
		--File_Cache_Object_Private : File_Cache.Object_Private_Type;
		--File_Cache_Object_Public  : File_Cache.Object_Public_Type;
	end record;

	procedure Internal_Read (Object : in out Object_Type;
	                         Packet : in     Packet_Type)
	with
		Pre =>
			Packet.Block_Count > 0 and
			not Packet_Buffer.Full(Object.Packet_Buffer_Object) and
			Packet.Block_Number + Sector_Type(Packet.Block_Count) < Max_Blocks;

end App.Block;
