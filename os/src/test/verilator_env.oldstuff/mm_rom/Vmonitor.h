// Verilated -*- C++ -*-
// DESCRIPTION: Verilator output: Primary design header
//
// This header should be included by all source files instantiating the design.
// The class here is then constructed to instantiate the design.
// See the Verilator manual for examples.

#ifndef _Vmonitor_H_
#define _Vmonitor_H_

#include "verilated.h"
class Vmonitor__Syms;

//----------

VL_MODULE(Vmonitor) {
  public:
    // CELLS
    // Public to allow access to /*verilator_public*/ items;
    // otherwise the application code can consider these internals.
    
    // PORTS
    // The application code writes and reads these signals to
    // propagate new values into/out from the Verilated model.
    VL_IN8(sys_clk,0,0);
    VL_IN8(sys_rst,0,0);
    VL_IN8(write_lock,0,0);
    VL_IN8(wb_sel_i,3,0);
    VL_IN8(wb_stb_i,0,0);
    VL_IN8(wb_cyc_i,0,0);
    VL_OUT8(wb_ack_o,0,0);
    VL_IN8(wb_we_i,0,0);
    VL_IN(wb_adr_i,31,0);
    VL_OUT(wb_dat_o,31,0);
    VL_IN(wb_dat_i,31,0);
    
    // LOCAL SIGNALS
    // Internals; generally not touched by application code
    VL_SIG(v__DOT__mem[512],31,0);
    
    // LOCAL VARIABLES
    // Internals; generally not touched by application code
    VL_SIG8(__Vclklast__TOP__sys_clk,0,0);
    //char	__VpadToAlign2077[3];
    
    // INTERNAL VARIABLES
    // Internals; generally not touched by application code
    //char	__VpadToAlign2084[4];
    Vmonitor__Syms*	__VlSymsp;		// Symbol table
    
    // PARAMETERS
    // Parameters marked /*verilator public*/ for use by application code
    
    // CONSTRUCTORS
  private:
    Vmonitor& operator= (const Vmonitor&);	///< Copying not allowed
    Vmonitor(const Vmonitor&);	///< Copying not allowed
  public:
    /// Construct the model; called by application code
    /// The special name  may be used to make a wrapper with a
    /// single model invisible WRT DPI scope names.
    Vmonitor(const char* name="TOP");
    /// Destroy the model; called (often implicitly) by application code
    ~Vmonitor();
    
    // USER METHODS
    
    // API METHODS
    /// Evaluate the model.  Application must call when inputs change.
    void eval();
    /// Simulation complete, run final blocks.  Application must call on completion.
    void final();
    
    // INTERNAL METHODS
  private:
    static void _eval_initial_loop(Vmonitor__Syms* __restrict vlSymsp);
  public:
    void __Vconfigure(Vmonitor__Syms* symsp, bool first);
  private:
    static IData	_change_request(Vmonitor__Syms* __restrict vlSymsp);
  public:
    static void	_eval(Vmonitor__Syms* __restrict vlSymsp);
    static void	_eval_initial(Vmonitor__Syms* __restrict vlSymsp);
    static void	_eval_settle(Vmonitor__Syms* __restrict vlSymsp);
    static void	_initial__TOP(Vmonitor__Syms* __restrict vlSymsp);
    static void	_sequent__TOP__1(Vmonitor__Syms* __restrict vlSymsp);
} VL_ATTR_ALIGNED(64);

#endif  /*guard*/
