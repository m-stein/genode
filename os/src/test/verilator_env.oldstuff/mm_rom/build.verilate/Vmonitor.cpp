// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Design implementation internals
// See Vmonitor.h for the primary calling header

#include "Vmonitor.h"          // For This
#include "Vmonitor__Syms.h"

//--------------------
// STATIC VARIABLES


//--------------------

VL_CTOR_IMP(Vmonitor) {
    Vmonitor__Syms* __restrict vlSymsp = __VlSymsp = new Vmonitor__Syms(this, name());
    Vmonitor* __restrict vlTOPp VL_ATTR_UNUSED = vlSymsp->TOPp;
    // Reset internal values
    
    // Reset structure values
    sys_clk = 0;
    sys_rst = 0;
    write_lock = 0;
    wb_adr_i = VL_RAND_RESET_I(32);
    wb_dat_o = VL_RAND_RESET_I(32);
    wb_dat_i = VL_RAND_RESET_I(32);
    wb_sel_i = VL_RAND_RESET_I(4);
    wb_stb_i = 0;
    wb_cyc_i = 0;
    wb_ack_o = VL_RAND_RESET_I(1);
    wb_we_i = 0;
    { int __Vi0=0; for (; __Vi0<512; ++__Vi0) {
	    v__DOT__mem[__Vi0] = VL_RAND_RESET_I(32);
    }}
    __Vclklast__TOP__sys_clk = 0;
}

void Vmonitor::__Vconfigure(Vmonitor__Syms* vlSymsp, bool first) {
    if (0 && first) {}  // Prevent unused
    this->__VlSymsp = vlSymsp;
}

Vmonitor::~Vmonitor() {
    delete __VlSymsp; __VlSymsp=NULL;
}

//--------------------


void Vmonitor::eval() {
    Vmonitor__Syms* __restrict vlSymsp = this->__VlSymsp; // Setup global symbol table
    Vmonitor* __restrict vlTOPp VL_ATTR_UNUSED = vlSymsp->TOPp;
    // Initialize
    if (VL_UNLIKELY(!vlSymsp->__Vm_didInit)) _eval_initial_loop(vlSymsp);
    // Evaluate till stable
    VL_DEBUG_IF(VL_PRINTF("\n----TOP Evaluate Vmonitor::eval\n"); );
    int __VclockLoop = 0;
    IData __Vchange=1;
    while (VL_LIKELY(__Vchange)) {
	VL_DEBUG_IF(VL_PRINTF(" Clock loop\n"););
	vlSymsp->__Vm_activity = true;
	_eval(vlSymsp);
	__Vchange = _change_request(vlSymsp);
	if (++__VclockLoop > 100) vl_fatal(__FILE__,__LINE__,__FILE__,"Verilated model didn't converge");
    }
}

void Vmonitor::_eval_initial_loop(Vmonitor__Syms* __restrict vlSymsp) {
    vlSymsp->__Vm_didInit = true;
    _eval_initial(vlSymsp);
    vlSymsp->__Vm_activity = true;
    int __VclockLoop = 0;
    IData __Vchange=1;
    while (VL_LIKELY(__Vchange)) {
	_eval_settle(vlSymsp);
	_eval(vlSymsp);
	__Vchange = _change_request(vlSymsp);
	if (++__VclockLoop > 100) vl_fatal(__FILE__,__LINE__,__FILE__,"Verilated model didn't DC converge");
    }
}

//--------------------
// Internal Methods

void Vmonitor::_initial__TOP(Vmonitor__Syms* __restrict vlSymsp) {
    VL_DEBUG_IF(VL_PRINTF("    Vmonitor::_initial__TOP\n"); );
    Vmonitor* __restrict vlTOPp VL_ATTR_UNUSED = vlSymsp->TOPp;
    // Variables
    //char	__VpadToAlign4[4];
    VL_SIGW(__Vtemp1,87,0,3);
    // Body
    // INITIAL at /home/hyronimo/GenodeLabs/milkymist/cores/monitor/rtl/monitor.v:42
    __Vtemp1[0] = 0x2e726f6d;
    __Vtemp1[1] = 0x69746f72;
    __Vtemp1[2] = 0x6d6f6e;
    VL_READMEM_W (true,32,512, 0,3, __Vtemp1, vlTOPp->v__DOT__mem
		  ,0,~0);
}

void Vmonitor::_sequent__TOP__1(Vmonitor__Syms* __restrict vlSymsp) {
    VL_DEBUG_IF(VL_PRINTF("    Vmonitor::_sequent__TOP__1\n"); );
    Vmonitor* __restrict vlTOPp VL_ATTR_UNUSED = vlSymsp->TOPp;
    // Variables
    VL_SIG8(__Vdly__wb_ack_o,0,0);
    VL_SIG8(__Vdlyvlsb__v__DOT__mem__v0,4,0);
    VL_SIG8(__Vdlyvval__v__DOT__mem__v0,7,0);
    VL_SIG8(__Vdlyvset__v__DOT__mem__v0,0,0);
    VL_SIG8(__Vdlyvlsb__v__DOT__mem__v1,4,0);
    VL_SIG8(__Vdlyvval__v__DOT__mem__v1,7,0);
    VL_SIG8(__Vdlyvset__v__DOT__mem__v1,0,0);
    VL_SIG8(__Vdlyvlsb__v__DOT__mem__v2,4,0);
    VL_SIG8(__Vdlyvval__v__DOT__mem__v2,7,0);
    VL_SIG8(__Vdlyvset__v__DOT__mem__v2,0,0);
    VL_SIG8(__Vdlyvlsb__v__DOT__mem__v3,4,0);
    VL_SIG8(__Vdlyvval__v__DOT__mem__v3,7,0);
    VL_SIG8(__Vdlyvset__v__DOT__mem__v3,0,0);
    //char	__VpadToAlign53[1];
    VL_SIG16(__Vdlyvdim0__v__DOT__mem__v0,8,0);
    VL_SIG16(__Vdlyvdim0__v__DOT__mem__v1,8,0);
    VL_SIG16(__Vdlyvdim0__v__DOT__mem__v2,8,0);
    VL_SIG16(__Vdlyvdim0__v__DOT__mem__v3,8,0);
    //char	__VpadToAlign62[2];
    // Body
    __Vdly__wb_ack_o = vlTOPp->wb_ack_o;
    __Vdlyvset__v__DOT__mem__v0 = 0;
    __Vdlyvset__v__DOT__mem__v1 = 0;
    __Vdlyvset__v__DOT__mem__v2 = 0;
    __Vdlyvset__v__DOT__mem__v3 = 0;
    // ALWAYS at /home/hyronimo/GenodeLabs/milkymist/cores/monitor/rtl/monitor.v:56
    vlTOPp->wb_dat_o = vlTOPp->v__DOT__mem[(0x1ff & 
					    (vlTOPp->wb_adr_i 
					     >> 2))];
    if (vlTOPp->sys_rst) {
	__Vdly__wb_ack_o = 0;
    } else {
	__Vdly__wb_ack_o = 0;
	if ((((IData)(vlTOPp->wb_stb_i) & (IData)(vlTOPp->wb_cyc_i)) 
	     & (~ (IData)(vlTOPp->wb_ack_o)))) {
	    if (((IData)(vlTOPp->wb_we_i) & ((3 == 
					      (3 & 
					       (vlTOPp->wb_adr_i 
						>> 9))) 
					     | (~ (IData)(vlTOPp->write_lock))))) {
		if ((1 & (IData)(vlTOPp->wb_sel_i))) {
		    __Vdlyvval__v__DOT__mem__v0 = (0xff 
						   & vlTOPp->wb_dat_i);
		    __Vdlyvset__v__DOT__mem__v0 = 1;
		    __Vdlyvlsb__v__DOT__mem__v0 = 0;
		    __Vdlyvdim0__v__DOT__mem__v0 = 
			(0x1ff & (vlTOPp->wb_adr_i 
				  >> 2));
		}
		if ((2 & (IData)(vlTOPp->wb_sel_i))) {
		    __Vdlyvval__v__DOT__mem__v1 = (0xff 
						   & (vlTOPp->wb_dat_i 
						      >> 8));
		    __Vdlyvset__v__DOT__mem__v1 = 1;
		    __Vdlyvlsb__v__DOT__mem__v1 = 8;
		    __Vdlyvdim0__v__DOT__mem__v1 = 
			(0x1ff & (vlTOPp->wb_adr_i 
				  >> 2));
		}
		if ((4 & (IData)(vlTOPp->wb_sel_i))) {
		    __Vdlyvval__v__DOT__mem__v2 = (0xff 
						   & (vlTOPp->wb_dat_i 
						      >> 0x10));
		    __Vdlyvset__v__DOT__mem__v2 = 1;
		    __Vdlyvlsb__v__DOT__mem__v2 = 0x10;
		    __Vdlyvdim0__v__DOT__mem__v2 = 
			(0x1ff & (vlTOPp->wb_adr_i 
				  >> 2));
		}
		if ((8 & (IData)(vlTOPp->wb_sel_i))) {
		    __Vdlyvval__v__DOT__mem__v3 = (0xff 
						   & (vlTOPp->wb_dat_i 
						      >> 0x18));
		    __Vdlyvset__v__DOT__mem__v3 = 1;
		    __Vdlyvlsb__v__DOT__mem__v3 = 0x18;
		    __Vdlyvdim0__v__DOT__mem__v3 = 
			(0x1ff & (vlTOPp->wb_adr_i 
				  >> 2));
		}
	    }
	    __Vdly__wb_ack_o = 1;
	}
    }
    vlTOPp->wb_ack_o = __Vdly__wb_ack_o;
    // ALWAYSPOST at /home/hyronimo/GenodeLabs/milkymist/cores/monitor/rtl/monitor.v:66
    if (__Vdlyvset__v__DOT__mem__v0) {
	vlTOPp->v__DOT__mem[(IData)(__Vdlyvdim0__v__DOT__mem__v0)] 
	    = (((~ ((IData)(0xff) << (IData)(__Vdlyvlsb__v__DOT__mem__v0))) 
		& vlTOPp->v__DOT__mem[(IData)(__Vdlyvdim0__v__DOT__mem__v0)]) 
	       | ((IData)(__Vdlyvval__v__DOT__mem__v0) 
		  << (IData)(__Vdlyvlsb__v__DOT__mem__v0)));
    }
    if (__Vdlyvset__v__DOT__mem__v1) {
	vlTOPp->v__DOT__mem[(IData)(__Vdlyvdim0__v__DOT__mem__v1)] 
	    = (((~ ((IData)(0xff) << (IData)(__Vdlyvlsb__v__DOT__mem__v1))) 
		& vlTOPp->v__DOT__mem[(IData)(__Vdlyvdim0__v__DOT__mem__v1)]) 
	       | ((IData)(__Vdlyvval__v__DOT__mem__v1) 
		  << (IData)(__Vdlyvlsb__v__DOT__mem__v1)));
    }
    if (__Vdlyvset__v__DOT__mem__v2) {
	vlTOPp->v__DOT__mem[(IData)(__Vdlyvdim0__v__DOT__mem__v2)] 
	    = (((~ ((IData)(0xff) << (IData)(__Vdlyvlsb__v__DOT__mem__v2))) 
		& vlTOPp->v__DOT__mem[(IData)(__Vdlyvdim0__v__DOT__mem__v2)]) 
	       | ((IData)(__Vdlyvval__v__DOT__mem__v2) 
		  << (IData)(__Vdlyvlsb__v__DOT__mem__v2)));
    }
    if (__Vdlyvset__v__DOT__mem__v3) {
	vlTOPp->v__DOT__mem[(IData)(__Vdlyvdim0__v__DOT__mem__v3)] 
	    = (((~ ((IData)(0xff) << (IData)(__Vdlyvlsb__v__DOT__mem__v3))) 
		& vlTOPp->v__DOT__mem[(IData)(__Vdlyvdim0__v__DOT__mem__v3)]) 
	       | ((IData)(__Vdlyvval__v__DOT__mem__v3) 
		  << (IData)(__Vdlyvlsb__v__DOT__mem__v3)));
    }
}

void Vmonitor::_eval(Vmonitor__Syms* __restrict vlSymsp) {
    VL_DEBUG_IF(VL_PRINTF("    Vmonitor::_eval\n"); );
    Vmonitor* __restrict vlTOPp VL_ATTR_UNUSED = vlSymsp->TOPp;
    // Body
    if (((IData)(vlTOPp->sys_clk) & (~ (IData)(vlTOPp->__Vclklast__TOP__sys_clk)))) {
	vlTOPp->_sequent__TOP__1(vlSymsp);
    }
    // Final
    vlTOPp->__Vclklast__TOP__sys_clk = vlTOPp->sys_clk;
}

void Vmonitor::_eval_initial(Vmonitor__Syms* __restrict vlSymsp) {
    VL_DEBUG_IF(VL_PRINTF("    Vmonitor::_eval_initial\n"); );
    Vmonitor* __restrict vlTOPp VL_ATTR_UNUSED = vlSymsp->TOPp;
    // Body
    vlTOPp->_initial__TOP(vlSymsp);
}

void Vmonitor::final() {
    VL_DEBUG_IF(VL_PRINTF("    Vmonitor::final\n"); );
    // Variables
    Vmonitor__Syms* __restrict vlSymsp = this->__VlSymsp;
    Vmonitor* __restrict vlTOPp VL_ATTR_UNUSED = vlSymsp->TOPp;
}

void Vmonitor::_eval_settle(Vmonitor__Syms* __restrict vlSymsp) {
    VL_DEBUG_IF(VL_PRINTF("    Vmonitor::_eval_settle\n"); );
    Vmonitor* __restrict vlTOPp VL_ATTR_UNUSED = vlSymsp->TOPp;
}

IData Vmonitor::_change_request(Vmonitor__Syms* __restrict vlSymsp) {
    VL_DEBUG_IF(VL_PRINTF("    Vmonitor::_change_request\n"); );
    Vmonitor* __restrict vlTOPp VL_ATTR_UNUSED = vlSymsp->TOPp;
    // Body
    // Change detection
    IData __req = false;  // Logically a bool
    return __req;
}
