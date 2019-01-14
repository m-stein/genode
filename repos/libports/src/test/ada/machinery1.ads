package Machinery1 is

   pragma Pure;

   type Temperature_Type is mod 2**32 with Size => 32;

   type Machinery1_Type is private;

   procedure Initialize (Machinery1 : out Machinery1_Type)
     with Export,
          Convention    => C,
          External_Name => "_ZN5Spark9Machinery1C1Ev";

   function Temperature (Machinery1 : Machinery1_Type) return Temperature_Type
     with Export,
          Convention    => C,
          External_Name => "_ZNK5Spark9Machinery111temperatureEv";

   procedure Heat_up (Machinery1 : in out Machinery1_Type)
     with Export,
          Convention    => C,
          External_Name => "_ZN5Spark9Machinery17heat_upEv";

private

   type Machinery1_Type is record
      Temperature : Temperature_Type;
   end record with Size => 32;

end Machinery1;
