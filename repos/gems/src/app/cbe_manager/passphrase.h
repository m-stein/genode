/*
 * \brief  Text buffer for a passphrase
 * \author Norman Feske
 * \author Martin Stein
 * \date   2021-03-02
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PASSPHRASE_H_
#define _PASSPHRASE_H_

/* Genode includes */
#include <base/output.h>
#include <util/utf8.h>

/* local includes */
#include <types.h>

namespace Cbe_manager {

	class Blind_passphrase;
	class Passphrase;
}


class Cbe_manager::Blind_passphrase
{
	public:

		virtual void print_bullets(Output &) const = 0;

		virtual ~Blind_passphrase() { }

		void print(Output &out) const { print_bullets(out); }
};


class Cbe_manager::Passphrase : Blind_passphrase
{
	public:

		enum { MAX_LENGTH = 64 };

	private:

		Codepoint _characters[MAX_LENGTH] { };

		unsigned _length = 0;

	public:

		/**
		 * Print PSK as UTF-8 string
		 */
		void print(Output &out) const
		{
			/*
			 * XXX duplicated from gems/src/server/terminal/main.cc
			 */
			struct Utf8 { char b0, b1, b2, b3, b4; };

			auto utf8_from_codepoint = [] (unsigned c) {

				/* extract 'n' bits 'at' bit position of value 'c' */
				auto bits = [c] (unsigned at, unsigned n) {
					return (c >> at) & ((1 << n) - 1); };

				return (c < 2<<7)  ? Utf8 { char(bits( 0, 7)), 0, 0, 0, 0 }
				     : (c < 2<<11) ? Utf8 { char(bits( 6, 5) | 0xc0),
				                            char(bits( 0, 6) | 0x80), 0, 0, 0 }
				     : (c < 2<<16) ? Utf8 { char(bits(12, 4) | 0xe0),
				                            char(bits( 6, 6) | 0x80),
				                            char(bits( 0, 6) | 0x80), 0, 0 }
				     : (c < 2<<21) ? Utf8 { char(bits(18, 3) | 0xf0),
				                            char(bits(12, 6) | 0x80),
				                            char(bits( 6, 6) | 0x80),
				                            char(bits( 0, 6) | 0x80), 0 }
				     : Utf8 { };
			};

			for (unsigned i = 0; i < _length; i++) {

				Utf8 const utf8 = utf8_from_codepoint(_characters[i].value);

				auto _print = [&] (char c) {
					if (c)
						Genode::print(out, Char(c)); };

				_print(utf8.b0); _print(utf8.b1); _print(utf8.b2);
				_print(utf8.b3); _print(utf8.b4);
			}
		}

		void append_character(Codepoint c)
		{
			if (_length < MAX_LENGTH) {
				_characters[_length] = c;
				_length++;
			}
		}

		void remove_last_character()
		{
			if (_length > 0) {
				_length--;
				_characters[_length].value = 0;
			}
		}

		/**
		 * Print passphrase as a number of bullets
		 */
		void print_bullets(Output &out) const override
		{
			char const bullet_utf8[4] = { (char)0xe2, (char)0x80, (char)0xa2, 0 };
			for (unsigned i = 0; i < _length; i++)
				Genode::print(out, bullet_utf8);
		}

		bool suitable() const
		{
			return _length >= 1;
		}

		Number_of_bytes to_nr_of_bytes() const
		{
			String<32> const str { *this };
			Number_of_bytes result { 0 };
			ascii_to(str.string(), result);
			return result;
		}

		unsigned long to_unsigned_long() const
		{
			String<32> const str { *this };
			unsigned long result { 0 };
			ascii_to(str.string(), result);
			return result;
		}

		bool is_nr_of_bytes_greater_than_zero() const
		{
			return (size_t)to_nr_of_bytes() > 0;
		}

		bool is_nr_greater_than_zero() const
		{
			return (size_t)to_unsigned_long() > 0;
		}

		char const *not_suitable_text() const
		{
			return "Must have at least 8 characters!";
		}

		unsigned length() const { return _length; }

		Blind_passphrase &blind() { return *this; }

		Passphrase()
		{
			/*
			 * FIXME Begin "For testing purpose only"
			 */
/*
			Codepoint c { };
			c.value = 0x58;
			append_character(c);
			append_character(c);
			append_character(c);
			append_character(c);
			append_character(c);
			append_character(c);
			append_character(c);
			append_character(c);
*/
			/*
			 * FIXME End
			 */
		}
};

#endif /* _PASSPHRASE_H_ */