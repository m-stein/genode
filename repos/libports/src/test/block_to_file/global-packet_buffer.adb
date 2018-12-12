pragma Ada_2012;

package body Global.Packet_Buffer with Spark_Mode is
   
   
   procedure Insert (Object : in out Object_Type;
                     Packet : in     Packet_Type) is begin
      
      Loop_Slots_Used :
      for Slot_Index in Slot_Index_Type loop
         
         if not Object.Slots_Used(Slot_Index) then
            Object.Slots(Slot_Index) := Packet;
            Object.Slots_Used(Slot_Index) := True;
            return;
         end if;
         
         -- all slots up to this point are in use
         pragma Loop_Invariant
           (for all Slot_Index_1 in Slot_Index_Type'First..Slot_Index =>
              Object.Slots_Used(Slot_Index_1) = True);
         
      end loop Loop_Slots_Used;
      
   end Insert;
   
   
   function Full(Object : in Object_Type) return Boolean is begin
      
      Loop_Slots_Used :
      for Slot_Index in Slot_Index_Type loop
         
         if not Object.Slots_Used(Slot_Index) then
            return False;
         end if;

         -- all slots up to this point are in use
         pragma Loop_Invariant
           (for all Slot_Index_1 in Slot_Index_Type'First..Slot_Index =>
              Object.Slots_Used(Slot_Index_1) = True);
         
      end loop Loop_Slots_Used;
      
      return True;
      
   end Full;
   
end Global.Packet_Buffer;
