pragma Ada_2012;

package body App.Packet_Buffer with Spark_Mode is
   
   procedure Insert (Object : in out Object_Type;
                     Packet : in     Packet_Type) is begin
      
      Loop_Slot_Array : for Index in Object.Slot_Array'Range loop
         
         if not Object.Slot_Array(Index).Used then
            Object.Slot_Array(Index) := Slot_Type'(Used   => True,
                                                   Packet => Packet
                                                  );
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
