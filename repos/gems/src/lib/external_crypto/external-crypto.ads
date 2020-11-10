--
--  Copyright (C) 2019 Genode Labs GmbH, Componolit GmbH, secunet AG
--
--  This file is part of the Consistent Block Encrypter project, which is
--  distributed under the terms of the GNU Affero General Public License
--  version 3.
--

pragma Ada_2012;

with CBE.Request;
with Aes_Cbc_4k;

package External.Crypto
with SPARK_Mode
is
   --  FIXME Not pure yet because of libsparkcrypto
   --  pragma Pure;

   Nr_Of_Keys : constant := 2;

   type Keys_Index_Type is range 0 .. Nr_Of_Keys - 1;

   subtype Key_Data_Type is Aes_Cbc_4k.Key_Type;

   subtype Plain_Data_Type is Aes_Cbc_4k.Plaintext_Type;

   subtype Cipher_Data_Type is Aes_Cbc_4k.Ciphertext_Type;

   type Object_Type is private;

   --
   --  Initialize_Object
   --
   procedure Initialize_Object (Obj : out Object_Type);

   --
   --  Initialized_Object
   --
   function Initialized_Object
   return Object_Type;

   --
   --  Add_Key
   --
   procedure Add_Key (
      Obj      : in out Object_Type;
      Key_ID   :        CBE.Key_ID_Type;
      Key_Data :        Key_Data_Type);

   --
   --  Execute
   --
   procedure Execute (
      Obj      : in out Object_Type;
      Progress :    out Boolean);

   --
   --  Encryption_Request_Acceptable
   --
   function Encryption_Request_Acceptable (Obj : Object_Type)
   return Boolean;

   --
   --  Decryption_Request_Acceptable
   --
   function Decryption_Request_Acceptable (Obj : Object_Type)
   return Boolean;

   --
   --  Submit_Encryption_Request
   --
   procedure Submit_Encryption_Request (
      Obj        : in out Object_Type;
      Request    :        CBE.Request.Object_Type;
      Plain_Data :        Plain_Data_Type);

   --
   --  Submit_Decryption_Request
   --
   procedure Submit_Decryption_Request (
      Obj         : in out Object_Type;
      Request     :        CBE.Request.Object_Type;
      Cipher_Data :        Cipher_Data_Type);

   --
   --  Peek_Completed_Encryption_Request
   --
   function Peek_Completed_Encryption_Request (Obj : Object_Type)
   return CBE.Request.Object_Type;

   --
   --  Peek_Completed_Decryption_Request
   --
   function Peek_Completed_Decryption_Request (Obj : Object_Type)
   return CBE.Request.Object_Type;

   --
   --  Supply_Cipher_Data
   --
   procedure Supply_Cipher_Data (
      Obj         : in out Object_Type;
      Request     :        CBE.Request.Object_Type;
      Cipher_Data :    out Cipher_Data_Type;
      Success     :    out Boolean);

   --
   --  Supply_Plain_Data
   --
   procedure Supply_Plain_Data (
      Obj        : in out Object_Type;
      Request    :        CBE.Request.Object_Type;
      Plain_Data :    out Plain_Data_Type;
      Success    :    out Boolean);

   --
   --  Remove_Key
   --
   procedure Remove_Key (
      Obj    : in out Object_Type;
      Key_ID :        CBE.Key_ID_Type);

private

   Nr_Of_Jobs : constant := 2;

   type Job_Operation_Type is (Invalid, Decrypt, Encrypt);

   type Job_State_Type is (Submitted, Completed);

   type Job_Type is record
      Operation   : Job_Operation_Type;
      State       : Job_State_Type;
      Request     : CBE.Request.Object_Type;
      Cipher_Data : Cipher_Data_Type;
      Plain_Data  : Plain_Data_Type;
   end record;

   type Jobs_Index_Type is range 0 .. Nr_Of_Jobs - 1;

   type Jobs_Type is array (Jobs_Index_Type) of Job_Type;

   type Key_Type is record
      Data  : Key_Data_Type;
      ID    : CBE.Key_ID_Type;
      Valid : Boolean;
   end record;

   --
   --  Key_Invalidate
   --
   procedure Key_Invalidate (Key : out Key_Type);

   type Keys_Type is array (Keys_Index_Type) of Key_Type;

   type Object_Type is record
      Jobs : Jobs_Type;
      Keys : Keys_Type;
   end record;

   --
   --  Execute_Decrypt
   --
   procedure Execute_Decrypt (
      Job      : in out Job_Type;
      Keys     :        Keys_Type;
      Progress : in out Boolean);

   --
   --  Execute_Encrypt
   --
   procedure Execute_Encrypt (
      Job      : in out Job_Type;
      Keys     :        Keys_Type;
      Progress : in out Boolean);

   --
   --  Invalid_Key
   --
   function Invalid_Key
   return Key_Type;

   --
   --  Invalid_Job
   --
   function Invalid_Job
   return Job_Type;

   --
   --  Key_Idx_For_Job
   --
   function Key_Idx_For_Job (
      Job  : Job_Type;
      Keys : Keys_Type)
   return Keys_Index_Type;

end External.Crypto;
