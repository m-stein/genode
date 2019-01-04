pragma Ada_2012;

package body Request_Buffer with Spark_Mode is


	procedure Initialize_Object(Object : out Object_Type) is begin

		Loop_Request_Array : for Index in Object.Request_Array'Range loop

			-- initialize all members of current Request
			Object.Request_Array(Index) :=
				Request_Type'(Operation    => Invalid,
				              Success      => C_False,
				              Offset       => 0,
				              Size         => 0);

		end loop Loop_Request_Array;

	end Initialize_Object;


	procedure Insert (Object  : in out Object_Type;
	                  Request : in     Request_Type)
	is begin

		Loop_Request_Array : for Index in Object.Request_Array'Range loop

			-- if Request is not in use, store Request in it and exit loop
			if not Request_Valid(Object.Request_Array(Index)) then
				Object.Request_Array(Index) := Request;
				Log("Buffered block request:");
				Log_Unsigned_Long(Index);
				exit Loop_Request_Array;
			end if;

			-- all processed Requests are in use
			pragma Loop_Invariant
				(for all Request of Object.Request_Array(Object.Request_Array'First .. Index) =>
					Request.Operation /= Invalid);

			-- some of the remaining Requests are not in use
			pragma Loop_Invariant
				(for some Request of Object.Request_Array(Index+1 .. Object.Request_Array'Last) =>
					not Request_Valid(Request));

		end loop Loop_Request_Array;

	end Insert;


	function Request_In_Buffer(Object  : in Object_Type;
	                           Request : in Request_Type) return Boolean
	is
		(for some Request_1 of Object.Request_Array => Request_1 = Request);


	function Full(Object : in Object_Type) return Boolean is
		(for all Request of Object.Request_Array => Request_Valid(Request));


end Request_Buffer;
