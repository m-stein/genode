pragma Ada_2012;

package Global.Packet_Buffer with Spark_Mode is
   
   type Slot_Index_Type is new Positive range 1..1024;
   type Slots_Type is array (Slot_Index_Type) of Packet_Type;
   type Slots_Used_Type is array (Slot_Index_Type) of Boolean;
   
   type Object_Type is record
      Slots      : Slots_Type;
      Slots_Used : Slots_Used_Type := (others => False);
   end record;
   
   procedure Insert(Object : in out Object_Type;
                    Packet : in     Packet_Type) with
     Pre =>
   -- one of the slots is not in use
     (for some Slot_Index in Slot_Index_Type =>
        not Object.Slots_Used(Slot_Index)),
     Post =>
   -- one of the slots is in use and contains the new packet
     (for some Slot_Index in Slot_Index_Type =>
        Object.Slots_Used(Slot_Index)),
   -- and Object.Slots(Slot_Index) = Packet),
     Global => null;
   
   function Full(Object : in Object_Type) return Boolean with
     Post =>
   --- if true is returned, all slots are in use
     (if Full'Result then
        (for all Slot_Index_1 in Slot_Index_Type =>
             Object.Slots_Used(Slot_Index_1))
          else
          --- otherwise. one slot is not in use
        (for some Slot_Index in Slot_Index_Type =>
             not Object.Slots_Used(Slot_Index))),
     Global => null;

end Global.Packet_Buffer;
--      and
--     -- all slots before the one with the new packet are in use
--     (for all Slot_Index_2 in Slot_Index_Type'First..Slot_Index_1 =>
--        Slots_Used(Slot_Index_2) = True)
