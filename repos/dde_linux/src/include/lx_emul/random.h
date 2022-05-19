/**
 * \brief  Source of randomness in lx_emul
 * \author Josef Soentgen
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2022-05-19
 *
 * For all intents and purposes this random back end should be treated
 * as a gloryfied counter.
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__RANDOM_H_
#define _LX_EMUL__RANDOM_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Write a certain number of consecutive, random byte values beginning at
 * a given address.
 */
void lx_emul_gen_random_bytes(void          *dst,
                              unsigned long  nr_of_bytes);

/**
 * Return a random unsigned integer value.
 */
unsigned int lx_emul_gen_random_uint(void);

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__RANDOM_H_ */
