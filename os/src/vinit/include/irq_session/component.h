/*
 * \brief  Session component of an emulated IRQ service
 * \author Martin Stein
 * \date   2012-06-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__IRQ_SESSION__COMPONENT_H_
#define _INCLUDE__IRQ_SESSION__COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <base/rpc_client.h>
#include <base/signal.h>
#include <irq_session/irq_session.h>
#include <irq_session/capability.h>
#include <util/list.h>

namespace Init
{
	using namespace Genode;

	/**
	 * Session component of an emulated IRQ service
	 */
	class Irq_session_component : public Rpc_object<Irq_session>,
	                              public List<Irq_session_component>::Element
	{
		private:

			struct Irq_control
			{
				GENODE_RPC(Rpc_associate_to_irq, void, associate_to_irq, unsigned);
				GENODE_RPC_INTERFACE(Rpc_associate_to_irq);
			};

			struct Irq_control_client : Rpc_client<Irq_control>
			{
					Irq_control_client(Capability<Irq_control> cap)
					: Rpc_client<Irq_control>(cap) { }

					void associate_to_irq(unsigned irq)
					{ call<Rpc_associate_to_irq>(irq); }
			};

			struct Irq_control_component : Rpc_object<Irq_control,
			                                          Irq_control_component>
			{
				void associate_to_irq(unsigned irq) { };
			};

			unsigned _irq;

			enum { STACK_SIZE = 2048 };
			Rpc_entrypoint _ep;


			/********************************************
			 ** IRQ control server (internal use only) **
			 ********************************************/

			Irq_control_component     _control_component;  /* ctrl component */
			Capability<Irq_control>   _control_cap;        /* capability for ctrl server */
			Irq_control_client        _control_client;     /* ctrl client */
			Capability<Irq_session>   _irq_cap;            /* capability for IRQ */
			Emulation::Session *      _emulation;
			Signal_receiver           _irq_receiver;
			Signal_context            _irq_edge;
			Signal_context_capability _irq_edge_cap;

		public:

			/**
			 * Constructor
			 *
			 * \param cap_session  capability session to use
			 * \param args         session construction arguments
			 */
			Irq_session_component(Cap_session * cap_session,
			                      const char  * args)
			:
				_ep(cap_session, STACK_SIZE, "irqctrl"),
				_control_cap(_ep.manage(&_control_component)),
				_control_client(_control_cap),
				_irq_edge_cap(_irq_receiver.manage(&_irq_edge))
			{
				/* fetch and validate session attributes */
				bool shared = Arg_string::find_arg(args, "irq_shared").bool_value(false);
				Arg irq_number = Arg_string::find_arg(args, "irq_number");
				_emulation = (Emulation::Session *)
					Arg_string::find_arg(args, emulation_key()).long_value(0);
				if (!_emulation || shared || !irq_number.valid()) {
					PERR("%s:%d: Invalid arguments", __FILE__, __LINE__);
					throw Root::Invalid_args();
				}
				_irq = irq_number.long_value(0);

				/* configure control client */
				_control_client.associate_to_irq(_irq);

				/* create IRQ capability */
				_irq_cap = Irq_session_capability(_ep.manage(this));
			}

			/**
			 * Destructor
			 */
			~Irq_session_component()
			{
				PERR("%s:%d: Not implemented", __FILE__, __LINE__);
				while (1) ;
			}

			/**
			 * Return capability to this session
			 *
			 * If an initialization error occurs, returned _cap is invalid.
			 */
			Capability<Irq_session> cap() const { return _irq_cap; }


			/***************************
			 ** Irq_session interface **
			 ***************************/

			void wait_for_irq()
			{
				/* start listening to the IRQ state of the emulator */
				bool irq_state = _emulation->irq_handler(_irq, _irq_edge_cap);
				if (!irq_state) _irq_receiver.wait_for_signal();

				/* stop listening to the IRQ state of the emulator */
				_emulation->irq_handler(_irq, Signal_context_capability());
			}
	};
}

#endif /* _INCLUDE__IRQ_SESSION__COMPONENT_H_ */

