--
--  Copyright (C) 2020 Genode Labs GmbH, Componolit GmbH, secunet AG
--
--  This file is part of the Consistent Block Encrypter project, which is
--  distributed under the terms of the GNU Affero General Public License
--  version 3.
--

pragma Ada_2012;

package body External.Trust_Anchor.CXX
with SPARK_Mode
is
   function Object_Size (Obj : External.Trust_Anchor.Anchor_Type)
   return CXX_Object_Size_Type is (Obj'Size / 8);

   procedure Initialize_Object (Anchor : out External.Trust_Anchor.Anchor_Type)
   is
   begin
      External.Trust_Anchor.Initialize_Anchor (Anchor);
   end Initialize_Object;

   function Request_Acceptable (Anchor : External.Trust_Anchor.Anchor_Type)
   return CXX_Bool_Type
   is
   begin
      return CXX_Bool_From_SPARK (
         External.Trust_Anchor.Request_Acceptable (Anchor));
   end Request_Acceptable;

   procedure Submit_Request (
      Anchor : in out External.Trust_Anchor.Anchor_Type;
      Req    :        CXX_TA_Request_Type)
   is
   begin
      External.Trust_Anchor.Submit_Request (Anchor,
         CXX_TA_Request_To_SPARK (Req));
   end Submit_Request;

   procedure Submit_Request_Key_Value_Plaintext (
      Anchor : in out External.Trust_Anchor.Anchor_Type;
      Req    :        CXX_TA_Request_Type;
      Key    :        CXX_Key_Value_Plaintext_Type)
   is
   begin
      External.Trust_Anchor.Submit_Request_Key_Value_Plaintext (
         Anchor,
         CXX_TA_Request_To_SPARK (Req),
         CXX_Key_Value_Plaintext_To_SPARK (Key));
   end Submit_Request_Key_Value_Plaintext;

   procedure Submit_Request_Key_Value_Ciphertext (
      Anchor : in out External.Trust_Anchor.Anchor_Type;
      Req    :        CXX_TA_Request_Type;
      Key    :        CXX_Key_Value_Ciphertext_Type)
   is
   begin
      External.Trust_Anchor.Submit_Request_Key_Value_Ciphertext (
         Anchor,
         CXX_TA_Request_To_SPARK (Req),
         CXX_Key_Value_Ciphertext_To_SPARK (Key));
   end Submit_Request_Key_Value_Ciphertext;

   procedure Submit_Request_Hash (
      Anchor : in out External.Trust_Anchor.Anchor_Type;
      Req    :        CXX_TA_Request_Type;
      Hash   :        CXX_Hash_Type)
   is
   begin
      External.Trust_Anchor.Submit_Request_Hash (
         Anchor,
         CXX_TA_Request_To_SPARK (Req),
         CXX_Hash_To_SPARK (Hash));
   end Submit_Request_Hash;

   function Peek_Completed_Request (Anchor : External.Trust_Anchor.Anchor_Type)
   return CXX_TA_Request_Type
   is
   begin
      return CXX_TA_Request_From_SPARK (
         External.Trust_Anchor.Peek_Completed_Request (Anchor));
   end Peek_Completed_Request;

   function Peek_Completed_Key_Value_Plaintext (
      Anchor : External.Trust_Anchor.Anchor_Type;
      Req    : CXX_TA_Request_Type)
   return CXX_Key_Value_Plaintext_Type
   is
   begin
      return CXX_Key_Value_Plaintext_From_SPARK (
         External.Trust_Anchor.Peek_Completed_Key_Value_Plaintext (
            Anchor,
            CXX_TA_Request_To_SPARK (Req)));
   end Peek_Completed_Key_Value_Plaintext;

   function Peek_Completed_Key_Value_Ciphertext (
      Anchor : External.Trust_Anchor.Anchor_Type;
      Req    : CXX_TA_Request_Type)
   return CXX_Key_Value_Ciphertext_Type
   is
   begin
      return CXX_Key_Value_Ciphertext_From_SPARK (
         External.Trust_Anchor.Peek_Completed_Key_Value_Ciphertext (
            Anchor,
            CXX_TA_Request_To_SPARK (Req)));
   end Peek_Completed_Key_Value_Ciphertext;

   procedure Drop_Completed_Request (
      Anchor : in out External.Trust_Anchor.Anchor_Type;
      Req    :        CXX_TA_Request_Type)
   is
   begin
      External.Trust_Anchor.Drop_Completed_Request (
         Anchor,
         CXX_TA_Request_To_SPARK (Req));
   end Drop_Completed_Request;

   procedure Execute (
      Anchor   : in out External.Trust_Anchor.Anchor_Type;
      Progress : in out CXX_Bool_Type)
   is
      SPARK_Bool : Boolean := False;
   begin
      External.Trust_Anchor.Execute (Anchor, SPARK_Bool);
      Progress := CXX_Bool_From_SPARK (SPARK_Bool);
   end Execute;

end External.Trust_Anchor.CXX;
