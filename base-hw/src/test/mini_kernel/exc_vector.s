.macro _exc_entry exc, off
	sub   lr, lr, #\off
	srsdb sp!, #19
	cps   #19
	stmdb sp!, {r0-r12}
	adr   lr, 1f
	mov   r0, #\exc
	b     exception_entry
1:
	ldmia sp!, {r0-r12}
	rfeia sp!
.endm


.p2align 5
.globl _exception_vector
_exception_vector:
		b _rst_entry      /* Reset                  */
		b _und_entry      /* Undefined instruction  */
		b _svc_entry      /* Supervisor call        */
		b _pab_entry      /* Prefetch abort         */
		b _dab_entry      /* Data abort             */
		nop               /* Reserved               */
		b _irq_entry      /* Interrupt request      */
		_exc_entry 7, 4   /* Fast interrupt request */

_rst_entry: _exc_entry 1, 0
_und_entry: _exc_entry 2, 4
_svc_entry: _exc_entry 3, 0
_pab_entry: _exc_entry 4, 4
_dab_entry: _exc_entry 5, 8
_irq_entry: _exc_entry 6, 4
