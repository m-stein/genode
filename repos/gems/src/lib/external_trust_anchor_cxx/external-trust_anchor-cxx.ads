--
--  Copyright (C) 2020 Genode Labs GmbH, Componolit GmbH, secunet AG
--
--  This file is part of the Consistent Block Encrypter project, which is
--  distributed under the terms of the GNU Affero General Public License
--  version 3.
--

pragma Ada_2012;

with CBE.CXX;
with External.Trust_Anchor;

package External.Trust_Anchor.CXX
with SPARK_Mode
is
   pragma Pure;

   use CBE.CXX;

   function Object_Size (Obj : External.Trust_Anchor.Anchor_Type)
   return CBE.CXX.CXX_Object_Size_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZN8External11object_sizeERKNS_12Trust_anchorE";

   procedure Initialize_Object (Anchor : out External.Trust_Anchor.Anchor_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN8External12Trust_anchorC1Ev";

   function Request_Acceptable (Anchor : External.Trust_Anchor.Anchor_Type)
   return CXX_Bool_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZNK8External12Trust_anchor18request_acceptableEv";

   procedure Submit_Request (
      Anchor : in out External.Trust_Anchor.Anchor_Type;
      Req    :        CXX_TA_Request_Type)
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN8External12Trust_anchor25submit_create_key_request" &
         "ERKN3Cbe20Trust_anchor_requestE";

   procedure Submit_Request_Key_Value_Plaintext (
      Anchor : in out External.Trust_Anchor.Anchor_Type;
      Req    :        CXX_TA_Request_Type;
      Key    :        CXX_Key_Value_Plaintext_Type)
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN8External12Trust_anchor26submit_encrypt_key_request" &
         "ERKN3Cbe20Trust_anchor_requestERKNS1_19Key_plaintext_valueE";

   procedure Submit_Request_Key_Value_Ciphertext (
      Anchor : in out External.Trust_Anchor.Anchor_Type;
      Req    :        CXX_TA_Request_Type;
      Key    :        CXX_Key_Value_Ciphertext_Type)
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN8External12Trust_anchor26submit_decrypt_key_request" &
         "ERKN3Cbe20Trust_anchor_requestERKNS1_20Key_ciphertext_valueE";

   procedure Submit_Request_Hash (
      Anchor : in out External.Trust_Anchor.Anchor_Type;
      Req    :        CXX_TA_Request_Type;
      Hash   :        CXX_Hash_Type)
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN8External12Trust_anchor32submit_secure_superblock_request" &
         "ERKN3Cbe20Trust_anchor_requestERKNS1_4HashE";

   function Peek_Completed_Request (Anchor : External.Trust_Anchor.Anchor_Type)
   return CXX_TA_Request_Type
   with
      Export,
      Convention    => C,
      External_Name => "_ZNK8External12Trust_anchor22peek_completed_requestEv";

   function Peek_Completed_Key_Value_Plaintext (
      Anchor : External.Trust_Anchor.Anchor_Type;
      Req    : CXX_TA_Request_Type)
   return CXX_Key_Value_Plaintext_Type
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZNK8External12Trust_anchor34peek_completed_key_value_plaintext" &
         "ERKN3Cbe20Trust_anchor_requestE";

   function Peek_Completed_Key_Value_Ciphertext (
      Anchor : External.Trust_Anchor.Anchor_Type;
      Req    : CXX_TA_Request_Type)
   return CXX_Key_Value_Ciphertext_Type
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZNK8External12Trust_anchor35peek_completed_key_value_ciphertext" &
         "ERKN3Cbe20Trust_anchor_requestE";

   procedure Drop_Completed_Request (
      Anchor : in out External.Trust_Anchor.Anchor_Type;
      Req    :        CXX_TA_Request_Type)
   with
      Export,
      Convention    => C,
      External_Name =>
         "_ZN8External12Trust_anchor22drop_completed_request" &
         "ERKN3Cbe20Trust_anchor_requestE";

   procedure Execute (
      Anchor   : in out External.Trust_Anchor.Anchor_Type;
      Progress : in out CXX_Bool_Type)
   with
      Export,
      Convention    => C,
      External_Name => "_ZN8External12Trust_anchor8_executeERb";

end External.Trust_Anchor.CXX;
