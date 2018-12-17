pragma Ada_2012;

with App.File;

package body App.File_Cache with Spark_Mode is

   procedure Read_Chunk(Object_Private : in out Object_Private_Type;
                        Object_Public  : in out Object_Public_Type;
                        Chunk_Number   : in     Chunk_Number_Type;
                        Chunk_In_Slot  : out    Boolean;
                        Slot_Index     : out    Slot_Array_Index_Type)
   is
      Object_Public.Slot_Array(1)'Address
   begin
      
      Chunk_In_Slot := False;
      
      Slot_Array_Loop : for Index in Object_Private.Slot_Array'Range loop
         if Object_Private.Slot_Array(Index).Chunk_Number = Chunk_Number then
            Chunk_In_Slot     := True;
            Slot_Index := Index;
            exit Slot_Array_Loop;
         end if;
      end loop Slot_Array_Loop;
      
      if not Chunk_In_Slot then
         
         App.File.Read(0, , Chunk_Type'Size / Byte'Size);
      end if;  
        
   end Read_Chunk;

end App.File_Cache;
