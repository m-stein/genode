
#ifndef _CRYPTA_H_
#define _CRYPTA_H_

namespace Cbe
{
	using Virtual_Block_Address_Type = Genode::addr_t;
	using Key_Plaintext_Type = Key_plaintext_value;
	using Block_IO_Data_Index_Type = unsigned long;
	using Jobs_Index_Type = unsigned long;
	using Key_ID_Type = unsigned long;
	using Cipher_Buffer_Index_Type = unsigned long;
	using Plain_Buffer_Index_Type = unsigned long;
	using Boolean = bool;

	class Primitive { };

	class Crypta_Job_Type
	{
		private:

			enum Job_State_Type
			{
				DSCD_Submitted,
				DSCD_Completed,

				DSCD_Decrypt_Data_Pending,
				DSCD_Decrypt_Data_In_Progress,
				DSCD_Decrypt_Data_Completed,

				DSCD_Supply_Data_Pending,
				DSCD_Supply_Data_In_Progress,
				DSCD_Supply_Data_Completed,

				OECD_Submitted,
				OECD_Completed,

				OECD_Encrypt_Data_Pending,
				OECD_Encrypt_Data_In_Progress,
				OECD_Encrypt_Data_Completed,

				OECD_Obtain_Data_Pending,
				OECD_Obtain_Data_In_Progress,
				OECD_Obtain_Data_Completed,

				Invalid,
				Pending,
				In_Progress,
				Complete
			};

			Job_State_Type             State           ;
			Primitive                  Prim            ;
			Primitive                  Submitted_Prim  ;
			Primitive                  Generated_Prim  ;
			Request                    Req             ;
			Virtual_Block_Address_Type VBA             ;
			Key_Plaintext_Type         Key             ;
			Block_IO_Data_Index_Type   Blk_IO_Data_Idx ;
	};

	class Crypta
	{
		private:

			using Job_Type = Crypta_Job_Type;

			Job_Type Jobs[1];

			//
			//  Execute_Decrypt_And_Supply_Client_Data
			//
			void Execute_Decrypt_And_Supply_Client_Data (
				Job_Type Job,
				Jobs_Index_Type Job_Idx,
				Boolean Progress);

			//
			//  Execute_Obtain_And_Encrypt_Client_Data
			//
			void Execute_Obtain_And_Encrypt_Client_Data (
				Job_Type Job,
				Jobs_Index_Type Job_Idx,
				Boolean Progress);

		public:

			//
			//  Initialized_Object
			//
			Crypta();

			//
			//  Primitive_Acceptable
			//
			Boolean Primitive_Acceptable ();

			//
			//  Submit_Primitive
			//
			void Submit_Primitive (
				Primitive Prim,
				Key_ID_Type Key_ID,
				Jobs_Index_Type Data_Idx);

			//
			//  Submit_Primitive_Key
			//
			void Submit_Primitive_Key (
				Primitive Prim,
				Key_Plaintext_Type Key);

			//
			//  Submit_Primitive_Key_ID
			//
			void Submit_Primitive_Key_ID (
				Primitive Prim,
				Key_ID_Type Key_ID);

			//
			//  Submit_Primitive_Encrypt_Client_Data
			//
			void Submit_Primitive_Encrypt_Client_Data (
				Primitive Prim,
				Request Req,
				Virtual_Block_Address_Type VBA,
				Key_ID_Type Key_ID,
				Block_IO_Data_Index_Type Blk_IO_Data_Idx);

			//
			//  Submit_Primitive_Decrypt_Client_Data
			//
			void Submit_Primitive_Decrypt_Client_Data (
				Primitive Prim,
				Request Req,
				Virtual_Block_Address_Type VBA,
				Key_ID_Type Key_ID,
				Cipher_Buffer_Index_Type Cipher_Buf_Idx);

			//
			//  Submit_Completed_Primitive
			//
			void Submit_Completed_Primitive (
				Primitive Prim,
				Key_ID_Type Key_ID,
				Jobs_Index_Type Data_Idx);

			//
			//  Peek_Generated_Primitive
			//
			void Peek_Generated_Primitive (
				Jobs_Index_Type Job_Idx,
				Primitive Prim);

			//
			//  Peek_Generated_Crypto_Dev_Primitive
			//
			Primitive Peek_Generated_Crypto_Dev_Primitive ();

			//
			//  Peek_Generated_Client_Primitive
			//
			Primitive Peek_Generated_Client_Primitive ();

			//
			//  Peek_Generated_Key_ID
			//
			Key_ID_Type Peek_Generated_Key_ID (
				Jobs_Index_Type Job_Idx);

			//
			//  Peek_Generated_Key_ID_New
			//
			Key_ID_Type Peek_Generated_Key_ID_New (
				Primitive Prim);

			//
			//  Peek_Generated_Cipher_Buf_Idx
			//
			Cipher_Buffer_Index_Type Peek_Generated_Cipher_Buf_Idx (
				Primitive Prim);

			//
			//  Peek_Generated_Plain_Buf_Idx
			//
			Plain_Buffer_Index_Type Peek_Generated_Plain_Buf_Idx (
				Primitive Prim);

			//
			//  Peek_Generated_Req
			//
			Request Peek_Generated_Req (
				Primitive Prim);

			//
			//  Peek_Generated_VBA
			//
			Virtual_Block_Address_Type Peek_Generated_VBA (
				Primitive Prim);

			//
			//  Peek_Generated_Key
			//
			Key_Plaintext_Type Peek_Generated_Key (
				Jobs_Index_Type Job_Idx);

			//
			//  Drop_Generated_Primitive
			//
			void Drop_Generated_Primitive (
				Jobs_Index_Type Job_Idx);

			//
			//  Drop_Generated_Primitive_New
			//
			void Drop_Generated_Primitive_New (
				Jobs_Index_Type Job_Idx);

			//
			//  Execute
			//
			void Execute (
				Boolean Progress);

			//
			//  Peek_Completed_Primitive
			//
			Primitive Peek_Completed_Primitive ();

			//
			//  Peek_Completed_Cipher_Buf_Idx
			//
			Jobs_Index_Type Peek_Completed_Cipher_Buf_Idx (
				Primitive Prim);

			//
			//  Peek_Completed_Blk_IO_Data_Idx
			//
			Block_IO_Data_Index_Type Peek_Completed_Blk_IO_Data_Idx (
				Primitive Prim);

			//
			//  Drop_Completed_Primitive
			//
			void Drop_Completed_Primitive ();

			//
			//  Drop_Completed_Primitive_New
			//
			void Drop_Completed_Primitive_New (
				Primitive Prim);

			//
			//  Mark_Completed_Primitive
			//
			void Mark_Completed_Primitive (
				Jobs_Index_Type Job_Idx,
				Boolean Success);

			//
			//  Mark_Generated_Primitive_Complete
			//
			void Mark_Generated_Primitive_Complete (
				Plain_Buffer_Index_Type Plain_Buf_Idx,
				Boolean Success);

			//
			//  Data_Index
			//
			Jobs_Index_Type Data_Index (
				Primitive Prim);
	};
}

#endif /* _CRYPTA_H_ */
