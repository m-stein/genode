/*
 * \brief  Trustzone specific VM-session declarations
 * \author Stefan Kalkowski
 * \date   2012-06-19
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE_HW__INCLUDE__BASE__VM_STATE_H_
#define _BASE_HW__INCLUDE__BASE__VM_STATE_H_

#include <base/stdint.h>

namespace Genode {

  struct Vm_state {
      addr_t r[13];

      addr_t sp_usr;
      addr_t lr_usr;

      addr_t sp_irq;
      addr_t lr_irq;
      addr_t spsr_irq;

      addr_t r_fiq[5];
      addr_t sp_fiq;
      addr_t lr_fiq;
      addr_t spsr_fiq;

      addr_t sp_abt;
      addr_t lr_abt;
      addr_t spsr_abt;

      addr_t sp_und;
      addr_t lr_und;
      addr_t spsr_und;

      addr_t sp_svc;
      addr_t lr_svc;
      addr_t spsr_svc;

      addr_t pc;
      addr_t cpsr;

      addr_t cp10_fpexc;

      addr_t exit_reason;
    };
}

#endif /* _BASE_HW__INCLUDE__BASE__VM_STATE_H_ */
