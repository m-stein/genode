pragma Ada_2012;

package body App.Packet_Buffer with Spark_Mode is


	procedure Initialize_Object(Object : out Object_Type) is begin

		Loop_Slot_Array : for Index in Object.Slot_Array'Range loop

			-- initialize all members of current slot
			--Object.Slot_Array(Index).Used   := False;
			Object.Slot_Array(Index).Packet :=
				Packet_Type'(Offset       => 0,
				             Size         => 0,
				             Operation    => Read,
				             Block_Number => 0,
				             Block_Count  => 0,
				             Success      => False);

		end loop Loop_Slot_Array;

	end Initialize_Object;


	procedure Insert (Object : in out Object_Type;
	                  Packet : in     Packet_Type)
	is begin

		Loop_Slot_Array : for Index in Object.Slot_Array'Range loop

			-- if slot is not in use, store packet in it and exit loop
			if not Object.Slot_Array(Index).Used then
				Object.Slot_Array(Index) := Slot_Type'(Used   => True,
				                                       Packet => Packet);

				Log("Buffered block request in slot:");
				Log_Unsigned_Long(Index);
				exit Loop_Slot_Array;
			end if;

			-- all processed slots are in use
			pragma Loop_Invariant
				(for all Slot of Object.Slot_Array(Object.Slot_Array'First .. Index) =>
					Slot.Used);

			-- some of the remaining slots are not in use
			pragma Loop_Invariant
				(for some Slot of Object.Slot_Array(Index+1 .. Object.Slot_Array'Last) =>
					not Slot.Used);

		end loop Loop_Slot_Array;

	end Insert;

end App.Packet_Buffer;
