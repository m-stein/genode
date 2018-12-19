pragma Ada_2012;

package App.Packet_Buffer with Spark_Mode is
   
   pragma Pure;
   
   type Object_Type is private;
   
   function Packet_In_Buffer(Object : in Object_Type;
                             Packet : in Packet_Type) return Boolean; -- with Ghost
   
   function Full(Object : in Object_Type) return Boolean;
      
   procedure Insert(Object : in out Object_Type;
                    Packet : in     Packet_Type)
     with
        Pre => not Full(Object),
        Post => Packet_In_Buffer(Object, Packet);
   
private
   
   type Slot_Type is record
      Used   : Boolean     := False;
      Packet : Packet_Type := Packet_Type'(Offset       => 0,
                                           Size         => 0,
                                           Operation    => Read,
                                           Block_Number => 0,
                                           Block_Count  => 0,
                                           Success      => False
                                          );
   end record;
   
   type Slot_Array_Type is array (Positive range 1..1024) of Slot_Type;
   type Object_Type is record
      Slot_Array : Slot_Array_Type;
   end record;
   
   function Packet_In_Buffer(Object : in Object_Type;
                             Packet : in Packet_Type) return Boolean
   is
     (for some Slot of Object.Slot_Array => Slot.Used and Slot.Packet = Packet);
   
   function Full(Object : in Object_Type) return Boolean is
     (for all Slot of Object.Slot_Array => Slot.Used);

end App.Packet_Buffer;
