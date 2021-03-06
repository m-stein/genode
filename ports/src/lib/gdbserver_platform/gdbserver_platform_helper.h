/*
 * \brief  Genode backend for GDBServer - helper functions
 * \author Christian Prochaska
 * \date   2011-07-07
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef GDBSERVER_PLATFORM_HELPER_H
#define GDBSERVER_PLATFORM_HELPER_H

/**
 * \throw Cpu_session::State_access_failed
 */
Genode::Thread_state get_current_thread_state();

#endif /* GDBSERVER_PLATFORM_HELPER_H */
