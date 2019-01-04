pragma Ada_2012;

with Request_Buffer;
with Utils;

use Utils;

package Fs_Block with Spark_Mode is

	pragma Pure;

	type Object_Type is private;

	-- maximum size of the block device as number of blocks
	Max_Device_Size : constant C_Size_Type := 1024 * 1024 * 1024;

	--
	-- Whether the given size is that of the object
	--
	function Correct_Object_Size(Object        : in Object_Type;
	                             Size_In_Bytes : in Natural) return Boolean;

	--
	-- Whether a request does access only valid block addresses
	--
	function Request_In_Device_Range(Request : in Request_Type) return Boolean;

	--
	-- Whether a request is in the request buffer
	--
	function Request_In_Buffer(Object  : in Object_Type;
	                           Request : in Request_Type) return Boolean;

	--
	-- Initialize an uninitialized package state
	--
	procedure Initialize_Object(Object        : out Object_Type;
	                            Size_In_Bytes : in  Natural)
	with
		Export, Convention => C, External_Name => "_ZN5Spark8Fs_blockC1Em",
		Pre => Correct_Object_Size(Object, Size_In_Bytes);

	--
	-- Wether new requests can be processed with the given package state
	--
	function Acceptable(Object : in Object_Type) return C_Boolean
	with
		Export, Convention => C, External_Name => "_ZNK5Spark8Fs_block10acceptableEv",
		Depends => (Acceptable'Result => Object);

	--
	-- Wether new requests can be processed with the given package state
	--
	procedure Submit(Object  : in out Object_Type;
	                 Request : in     Request_Type)
	with
		Export, Convention => C, External_Name => "_ZN5Spark8Fs_block6submitERN5Block7RequestE",
		Pre =>  Request_Valid(Request) and
		        Request.Size > 0 and
		        Request_In_Device_Range(Request) and
		        To_Spark_Boolean(Acceptable(Object)),
		Post => Request_In_Buffer(Object, Request);

private

	type Object_Type is record
		Request_Buffer_Object : Request_Buffer.Object_Type;
		--File_Cache_Object_Private : File_Cache.Object_Private_Type;
		--File_Cache_Object_Public  : File_Cache.Object_Public_Type;
	end record;

end Fs_Block;
