/*
 * \brief  Glue code between Genode C++ code and Wireguard C code
 * \author Martin Stein
 * \date   2022-01-07
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GLUE_H_
#define _GLUE_H_

enum {
	GLUE_KEY_LEN = 32,
};

typedef unsigned char  glue_uint8_t;
typedef unsigned short glue_uint16_t;

#endif /* _GLUE_H_ */
