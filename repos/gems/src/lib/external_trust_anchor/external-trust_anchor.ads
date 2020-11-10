--
--  Copyright (C) 2019 Genode Labs GmbH, Componolit GmbH, secunet AG
--
--  This file is part of the Consistent Block Encrypter project, which is
--  distributed under the terms of the GNU Affero General Public License
--  version 3.
--

pragma Ada_2012;

with CBE.TA_Request;

package External.Trust_Anchor
with SPARK_Mode
is
   pragma Pure;

   use CBE;

   type Anchor_Type is private;

   --
   --  Initialize_Anchor
   --
   procedure Initialize_Anchor (Anchor : out Anchor_Type);

   --
   --  Request_Acceptable
   --
   function Request_Acceptable (Anchor : Anchor_Type)
   return Boolean;

   --
   --  Submit_Request
   --
   procedure Submit_Request (
      Anchor : in out Anchor_Type;
      Req    :        TA_Request.Object_Type);

   --
   --  Submit_Request_Key_Value_Plaintext
   --
   procedure Submit_Request_Key_Value_Plaintext (
      Anchor : in out Anchor_Type;
      Req    :        TA_Request.Object_Type;
      Key    :        Key_Value_Plaintext_Type);

   --
   --  Submit_Request_Key_Value_Ciphertext
   --
   procedure Submit_Request_Key_Value_Ciphertext (
      Anchor : in out Anchor_Type;
      Req    :        TA_Request.Object_Type;
      Key    :        Key_Value_Ciphertext_Type);

   --
   --  Submit_Request_Hash
   --
   procedure Submit_Request_Hash (
      Anchor : in out Anchor_Type;
      Req    :        TA_Request.Object_Type;
      Hash   :        Hash_Type);

   --
   --  Peek_Completed_Request
   --
   function Peek_Completed_Request (Anchor : Anchor_Type)
   return TA_Request.Object_Type;

   --
   --  Peek_Completed_Key_Value_Plaintext
   --
   function Peek_Completed_Key_Value_Plaintext (
      Anchor : Anchor_Type;
      Req    : TA_Request.Object_Type)
   return Key_Value_Plaintext_Type;

   --
   --  Peek_Completed_Key_Value_Ciphertext
   --
   function Peek_Completed_Key_Value_Ciphertext (
      Anchor : Anchor_Type;
      Req    : TA_Request.Object_Type)
   return Key_Value_Ciphertext_Type;

   --
   --  Drop_Completed_Request
   --
   procedure Drop_Completed_Request (
      Anchor : in out Anchor_Type;
      Req    :        TA_Request.Object_Type);

   --
   --  Execute
   --
   procedure Execute (
      Anchor   : in out Anchor_Type;
      Progress : in out Boolean);

private

   Nr_Of_Jobs : constant := 2;

   type Jobs_Index_Type is range 0 .. Nr_Of_Jobs - 1;

   type Job_Operation_Type is (
      Invalid,
      Create_Key,
      Secure_Superblock,
      Encrypt_Key,
      Decrypt_Key);

   type Job_State_Type is (
      Submitted,
      Completed);

   type Job_Type is record
      Operation            : Job_Operation_Type;
      State                : Job_State_Type;
      Submitted_Req        : TA_Request.Object_Type;
      Key_Value_Plaintext  : Key_Value_Plaintext_Type;
      Key_Value_Ciphertext : Key_Value_Ciphertext_Type;
      Hash                 : Hash_Type;
   end record;

   type Jobs_Type is array (Jobs_Index_Type) of Job_Type;

   type Modulo_Byte_Type is mod 2**8;

   type Anchor_Type is record
      Jobs                          : Jobs_Type;
      Next_Key_Value_Plaintext_Byte : Modulo_Byte_Type;
      Secured_SB_Hash               : Hash_Type;
      Private_Key                   : Key_Value_Plaintext_Type;
   end record;

   --
   --  Execute_Secure_SB
   --
   procedure Execute_Secure_SB (
      Anchor   : in out Anchor_Type;
      Idx      :        Jobs_Index_Type;
      Progress : in out Boolean);

   --
   --  Execute_Create_Key
   --
   procedure Execute_Create_Key (
      Anchor   : in out Anchor_Type;
      Idx      :        Jobs_Index_Type;
      Progress : in out Boolean);

   --
   --  Execute_Encrypt_Key
   --
   procedure Execute_Encrypt_Key (
      Anchor   : in out Anchor_Type;
      Idx      :        Jobs_Index_Type;
      Progress : in out Boolean);

   --
   --  Execute_Decrypt_Key
   --
   procedure Execute_Decrypt_Key (
      Anchor   : in out Anchor_Type;
      Idx      :        Jobs_Index_Type;
      Progress : in out Boolean);

end External.Trust_Anchor;
