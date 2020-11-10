--
--  Copyright (C) 2019 Genode Labs GmbH, Componolit GmbH, secunet AG
--
--  This file is part of the Consistent Block Encrypter project, which is
--  distributed under the terms of the GNU Affero General Public License
--  version 3.
--

pragma Ada_2012;

with CBE.CXX;
with External.Crypto;

package External.Crypto.CXX
with SPARK_Mode
is
   --  pragma Pure;

   type CXX_Byte_Type     is range 0 .. 255 with Size => 8;
   type CXX_Key_Slot_Type is range 0 .. 2**32 - 1 with Size => 4 * 8;
   type CXX_Key_ID_Type   is range 0 .. 2**32 - 1 with Size => 4 * 8;
   type CXX_Key_Data_Type
   is array (0 .. 31) of CXX_Byte_Type with Size => 32 * 8;

   type CXX_Plain_Data_Type
   is array (0 .. 4095) of CXX_Byte_Type with Size => 4096 * 8;

   type CXX_Cipher_Data_Type
   is array (0 .. 4095) of CXX_Byte_Type with Size => 4096 * 8;

   function Object_Size (Obj : External.Crypto.Object_Type)
   return CBE.CXX.CXX_Object_Size_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZN8External11object_sizeERKNS_6CryptoE";

   procedure Initialize_Object (Obj : out External.Crypto.Object_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN8External6CryptoC1Ev";

   procedure Add_Key (
      Obj      : in out External.Crypto.Object_Type;
      Key_ID   :        CXX_Key_ID_Type;
      Key_Data :        External.Crypto.Key_Data_Type)
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN8External6Crypto7add_keyEN3Cbe3Key2IdERKNS0_8Key_dataE";

   procedure Remove_Key (
      Obj    : in out External.Crypto.Object_Type;
      Key_ID :        CXX_Key_ID_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN8External6Crypto10remove_keyEN3Cbe3Key2IdE";

   procedure Execute (
      Obj     : in out External.Crypto.Object_Type;
      Progess :    out CBE.CXX.CXX_Bool_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN8External6Crypto8_executeERb";

   function Encryption_Request_Acceptable (Obj : External.Crypto.Object_Type)
   return CBE.CXX.CXX_Bool_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZNK8External6Crypto29encryption_request_acceptableEv";

   procedure Submit_Encryption_Request (
      Obj  : in out External.Crypto.Object_Type;
      Req  :        CBE.CXX.CXX_Request_Type;
      Data :        External.Crypto.Plain_Data_Type;
      Res  :    out CBE.CXX.CXX_Bool_Type)
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN8External6Crypto26_submit_encryption_requestERKN3Cbe" &
         "7RequestERKNS1_10Block_dataERb";

   function Peek_Completed_Encryption_Request (
      Obj : External.Crypto.Object_Type)
   return CBE.CXX.CXX_Request_Type
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZNK8External6Crypto33peek_completed_encryption_requestEv";

   procedure Supply_Cipher_Data (
      Obj  : in out External.Crypto.Object_Type;
      Req  :        CBE.CXX.CXX_Request_Type;
      Data :    out External.Crypto.Cipher_Data_Type;
      Res  :    out CBE.CXX.CXX_Bool_Type)
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN8External6Crypto19_supply_cipher_dataERKN3Cbe7RequestERNS1_" &
         "10Block_dataERb";

   function Decryption_Request_Acceptable (Obj : External.Crypto.Object_Type)
   return CBE.CXX.CXX_Bool_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZNK8External6Crypto29decryption_request_acceptableEv";

   procedure Submit_Decryption_Request (
      Obj  : in out External.Crypto.Object_Type;
      Req  :        CBE.CXX.CXX_Request_Type;
      Data :        External.Crypto.Cipher_Data_Type;
      Res  :    out CBE.CXX.CXX_Bool_Type)
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN8External6Crypto26_submit_decryption_requestERKN3Cbe" &
         "7RequestERKNS1_10Block_dataERb";

   function Peek_Completed_Decryption_Request (
      Obj : External.Crypto.Object_Type)
   return CBE.CXX.CXX_Request_Type
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZNK8External6Crypto33peek_completed_decryption_requestEv";

   procedure Supply_Plain_Data (
      Obj  : in out External.Crypto.Object_Type;
      Req  :        CBE.CXX.CXX_Request_Type;
      Data :    out External.Crypto.Plain_Data_Type;
      Res  :    out CBE.CXX.CXX_Bool_Type)
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN8External6Crypto18_supply_plain_dataERKN3Cbe7RequestERNS1_" &
         "10Block_dataERb";

end External.Crypto.CXX;
