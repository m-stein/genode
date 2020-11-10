--
--  Copyright (C) 2019 Genode Labs GmbH, Componolit GmbH, secunet AG
--
--  This file is part of the Consistent Block Encrypter project, which is
--  distributed under the terms of the GNU Affero General Public License
--  version 3.
--

pragma Ada_2012;

package body External.Crypto.CXX
with SPARK_Mode
is
   use CBE.CXX;

   function Object_Size (Obj : External.Crypto.Object_Type)
   return CBE.CXX.CXX_Object_Size_Type is (Obj'Size / 8);

   procedure Initialize_Object (Obj : out External.Crypto.Object_Type)
   is
   begin
      External.Crypto.Initialize_Object (Obj);
   end Initialize_Object;

   procedure Add_Key (
      Obj      : in out External.Crypto.Object_Type;
      Key_ID   :        CXX_Key_ID_Type;
      Key_Data :        External.Crypto.Key_Data_Type)
   is
   begin
      External.Crypto.Add_Key (Obj, CBE.Key_ID_Type (Key_ID), Key_Data);
   end Add_Key;

   procedure Remove_Key (
      Obj    : in out External.Crypto.Object_Type;
      Key_ID :        CXX_Key_ID_Type)
   is
   begin
      External.Crypto.Remove_Key (Obj, CBE.Key_ID_Type (Key_ID));
   end Remove_Key;

   procedure Execute (
      Obj     : in out External.Crypto.Object_Type;
      Progess :    out CBE.CXX.CXX_Bool_Type)
   is
      SPARK_Progress : Boolean;
   begin
      External.Crypto.Execute (Obj, SPARK_Progress);
      Progess := CBE.CXX.CXX_Bool_From_SPARK (SPARK_Progress);
   end Execute;

   function Encryption_Request_Acceptable (Obj : External.Crypto.Object_Type)
   return CBE.CXX.CXX_Bool_Type
   is
   begin
      return CBE.CXX.CXX_Bool_From_SPARK (
         External.Crypto.Encryption_Request_Acceptable (Obj));
   end Encryption_Request_Acceptable;

   procedure Submit_Encryption_Request (
      Obj  : in out External.Crypto.Object_Type;
      Req  :        CBE.CXX.CXX_Request_Type;
      Data :        External.Crypto.Plain_Data_Type;
      Res  :    out CBE.CXX.CXX_Bool_Type)
   is
   begin
      External.Crypto.Submit_Encryption_Request (
         Obj, CBE.CXX.CXX_Request_To_SPARK (Req), Data);

      Res := CBE.CXX.CXX_Bool_From_SPARK (True);
   end Submit_Encryption_Request;

   function Peek_Completed_Encryption_Request (
      Obj : External.Crypto.Object_Type)
   return CBE.CXX.CXX_Request_Type
   is
   begin
      return CBE.CXX.CXX_Request_From_SPARK (
         External.Crypto.Peek_Completed_Encryption_Request (Obj));
   end Peek_Completed_Encryption_Request;

   procedure Supply_Cipher_Data (
      Obj  : in out External.Crypto.Object_Type;
      Req  :        CBE.CXX.CXX_Request_Type;
      Data :    out External.Crypto.Cipher_Data_Type;
      Res  :    out CBE.CXX.CXX_Bool_Type)
   is
      SPARK_Result : Boolean;
   begin
      External.Crypto.Supply_Cipher_Data (
         Obj, CBE.CXX.CXX_Request_To_SPARK (Req), Data, SPARK_Result);

      Res := CBE.CXX.CXX_Bool_From_SPARK (SPARK_Result);
   end Supply_Cipher_Data;

   function Decryption_Request_Acceptable (Obj : External.Crypto.Object_Type)
   return CBE.CXX.CXX_Bool_Type
   is
   begin
      return CBE.CXX.CXX_Bool_From_SPARK (
         External.Crypto.Decryption_Request_Acceptable (Obj));
   end Decryption_Request_Acceptable;

   procedure Submit_Decryption_Request (
      Obj  : in out External.Crypto.Object_Type;
      Req  :        CBE.CXX.CXX_Request_Type;
      Data :        External.Crypto.Cipher_Data_Type;
      Res  :    out CBE.CXX.CXX_Bool_Type)
   is
   begin
      External.Crypto.Submit_Decryption_Request (
         Obj, CBE.CXX.CXX_Request_To_SPARK (Req), Data);

      Res := CBE.CXX.CXX_Bool_From_SPARK (True);
   end Submit_Decryption_Request;

   function Peek_Completed_Decryption_Request (
      Obj : External.Crypto.Object_Type)
   return CBE.CXX.CXX_Request_Type
   is
   begin
      return CBE.CXX.CXX_Request_From_SPARK (
         External.Crypto.Peek_Completed_Decryption_Request (Obj));
   end Peek_Completed_Decryption_Request;

   procedure Supply_Plain_Data (
      Obj  : in out External.Crypto.Object_Type;
      Req  :        CBE.CXX.CXX_Request_Type;
      Data :    out External.Crypto.Plain_Data_Type;
      Res  :    out CBE.CXX.CXX_Bool_Type)
   is
      SPARK_Result : Boolean;
   begin
      External.Crypto.Supply_Plain_Data (
         Obj, CBE.CXX.CXX_Request_To_SPARK (Req), Data, SPARK_Result);

      Res := CBE.CXX.CXX_Bool_From_SPARK (SPARK_Result);
   end Supply_Plain_Data;

end External.Crypto.CXX;
