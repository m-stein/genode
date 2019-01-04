pragma Ada_2012;

with Utils;

use Utils;

package Request_Buffer with Spark_Mode is

	pragma Pure;

	--
	-- Type of package-state objects
	--
	type Object_Type is private;

	--
	-- Initialize an uninitialized package state
	--
	procedure Initialize_Object(Object : out Object_Type);

	--
	-- Whether a given request is currently stored in the buffer
	--
	function Request_In_Buffer(Object  : in Object_Type;
	                           Request : in Request_Type) return Boolean;
	-- with Ghost;

	--
	-- Whether the buffer cannot store more requests
	--
	function Full(Object : in Object_Type) return Boolean;

	--
	-- Store a new block request in a package state
	--
	procedure Insert(Object  : in out Object_Type;
	                 Request : in     Request_Type)
	with
		Pre => not Full(Object),
		Post => Request_In_Buffer(Object, Request);

private

	type Request_Array_Type is array (Positive range 1..1024) of Request_Type;

	type Object_Type is record
		Request_Array : Request_Array_Type;
	end record;

end Request_Buffer;
