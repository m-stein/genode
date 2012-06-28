.p2align 5
.globl _exception_vector
_exception_vector:
		b _exc_irq_entry
		b _exc_irq_entry   /* Undefined instruction  */
		b _exc_irq_entry   /* Supervisor call        */
		b _exc_irq_entry   /* Prefetch abort         */
		b _exc_irq_entry   /* Data abort             */
		b _exc_irq_entry   /* Reserved               */
		b _exc_irq_entry   /* Interrupt request      */
		b _exc_irq_entry   /* Fast interrupt request */

_exc_irq_entry:
		sub   lr, lr, #4
		srsdb sp!, #31
		cps   #31
		adr   lr, 1f
		b     exception_entry
1:
		rfeia sp!
