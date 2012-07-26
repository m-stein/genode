
#include <freehdl/kernel.h>
#include <freehdl/std.h>


/**
 * VHDL signal representation:
 *
 *   1 x signal reader (R<name_size><name>): stores current signal value
 *   N x signal driver (D<name_size><name>):
 *       list of transactions for the signal created by a source
 *       transaction: signalvalue + application timestamp
 *       transactions are ordered in list increasing by their timestamps
 */

/* external declarations */
/* end of external declarations */

/**************
 ** Entities **
 **************/

/**
 * Entity class for :work:adder
 */
class L4work_E5adder
{
	public:

		/* compound component */
		void * father_component;

		/**
		 * Constructor
		 */
		L4work_E5adder(name_stack & iname, map_list * mlist, void * father);

		/**
		 * Destructor
		 */
		~L4work_E5adder() { };

		/* signals */
		sig_info<enumeration> * L4work_E5adder_S7addend1,
		                      * L4work_E5adder_S7addend2,
		                      * L4work_E5adder_S6carryi,
		                      * L4work_E5adder_S3sum,
		                      * L4work_E5adder_S6carryo;
};


L4work_E5adder::
L4work_E5adder(name_stack &iname, map_list *mlist, void *father)
{
	father_component = father;
	iname.push("");

	/* create signal objects */
	L4work_E5adder_S7addend1 = new sig_info<enumeration>(iname,":addend1",":work:adder",mlist,(&L3std_Q8standard_I3bit_INFO),vIN,this);
	L4work_E5adder_S7addend2 = new sig_info<enumeration>(iname,":addend2",":work:adder",mlist,(&L3std_Q8standard_I3bit_INFO),vIN,this);
	L4work_E5adder_S6carryi = new sig_info<enumeration>(iname,":carryi",":work:adder",mlist,(&L3std_Q8standard_I3bit_INFO),vIN,this);
	L4work_E5adder_S3sum = new sig_info<enumeration>(iname,":sum",":work:adder",mlist,(&L3std_Q8standard_I3bit_INFO),vOUT,this);
	L4work_E5adder_S6carryo = new sig_info<enumeration>(iname,":carryo",":work:adder",mlist,(&L3std_Q8standard_I3bit_INFO),vOUT,this);

	iname.pop();
};


int L3std_Q8standard_init();
bool L4work_E5adder_init_done = false;


/**
 * Initialization function for entity :work:adder
 */
int L4work_E5adder_init()
{
	/* initialize only once */
	if (L4work_E5adder_init_done) return 1;
	L4work_E5adder_init_done=true;

	/* initialization */
	L3std_Q8standard_init();
	register_source_file("/home/hyronimo/LokaleDaten1/GenodeLabs/vhdl/examples/adder.vhdl","adder.vhdl");
	name_stack iname;
	iname.push("");
	iname.pop();
	return 1;
}

/* external declarations */
/* end of external declarations */


/*******************
 ** Architectures **
 *******************/

/**
 * Architecture class for :work:adder
 */
class L4work_E5adder_A3rtl : public L4work_E5adder {

	public:

		/**
		 * Constructor
		 */
		L4work_E5adder_A3rtl (name_stack &iname, map_list *mlist, void *father, int level);

		/**
		 * Destructor
		 */
		~L4work_E5adder_A3rtl();
};


/**************************
 ** Concurrent processes **
 **************************/

/**
 * Class for process _4pn
 */
class L4work_E5adder_A3rtl_P4_4pn : public process_base
{
	public:

		/**
		 * Constructor
		 */
		L4work_E5adder_A3rtl_P4_4pn(L4work_E5adder_A3rtl * L4work_E5adder_A3rtl_AP_PAR, name_stack & iname);

		/**
		 * Destructor
		 */
		~L4work_E5adder_A3rtl_P4_4pn() { };

		/**
		 * Execute process
		 */
		bool execute();

		/* architecture that defines this process */
		L4work_E5adder_A3rtl * L4work_E5adder_A3rtl_AP;

		/* left side of the statement */
		driver_info * L4work_E5adder_D3sum;

		/* right side of the statement */

		/* signal readers */
		enumeration * L4work_E5adder_R7addend1,
		            * L4work_E5adder_R7addend2,
		            * L4work_E5adder_R6carryi;

		/* signals */
		sig_info<enumeration> * L4work_E5adder_S7addend1,
		                      * L4work_E5adder_S7addend2,
		                      * L4work_E5adder_S6carryi;

		/* wait contexts */
		winfo_item winfo[1];

		/* ? */
		L4work_E5adder_A3rtl * arch;
};


L4work_E5adder_A3rtl_P4_4pn::
L4work_E5adder_A3rtl_P4_4pn(L4work_E5adder_A3rtl * L4work_E5adder_A3rtl_AP_PAR,
                            name_stack & iname)
: process_base(iname)
{
	L4work_E5adder_A3rtl_AP = L4work_E5adder_A3rtl_AP_PAR;
	L4work_E5adder_S7addend1 = L4work_E5adder_A3rtl_AP->L4work_E5adder_S7addend1;
	L4work_E5adder_S7addend2 = L4work_E5adder_A3rtl_AP->L4work_E5adder_S7addend2;
	L4work_E5adder_S6carryi = L4work_E5adder_A3rtl_AP->L4work_E5adder_S6carryi;
	L4work_E5adder_R7addend1 = &L4work_E5adder_A3rtl_AP->L4work_E5adder_S7addend1->reader();
	L4work_E5adder_R7addend2 = &L4work_E5adder_A3rtl_AP->L4work_E5adder_S7addend2->reader();
	L4work_E5adder_R6carryi = &L4work_E5adder_A3rtl_AP->L4work_E5adder_S6carryi->reader();
	L4work_E5adder_D3sum = kernel.get_driver(this, L4work_E5adder_A3rtl_AP->L4work_E5adder_S3sum);

	{
		/* setup info about signals we are sensitive to */
		sigacl_list sal(3);
		sal.add(L4work_E5adder_A3rtl_AP->L4work_E5adder_S7addend1);
		sal.add(L4work_E5adder_A3rtl_AP->L4work_E5adder_S7addend2);
		sal.add(L4work_E5adder_A3rtl_AP->L4work_E5adder_S6carryi);
		winfo[0] = kernel.setup_wait_info(sal, this);
	}
}


bool L4work_E5adder_A3rtl_P4_4pn::execute()
{
	L4work_E5adder_D3sum->inertial_assign(
		op_xor(
			op_xor(
				(*L4work_E5adder_R7addend1),
				(*L4work_E5adder_R7addend2)),
			(*L4work_E5adder_R6carryi)),
		vtime(0));
	return true;
}


/**
 * Class for process _5pn
 */
class L4work_E5adder_A3rtl_P4_5pn : public process_base
{
	public:

		/**
		 * Constructor
		 */
		L4work_E5adder_A3rtl_P4_5pn(L4work_E5adder_A3rtl * L4work_E5adder_A3rtl_AP_PAR, name_stack & iname);

		/**
		 * Destructor
		 */
		~L4work_E5adder_A3rtl_P4_5pn() { };

		/**
		 * Execute process
		 */
		bool execute();

		/* architecture that defines this process */
		L4work_E5adder_A3rtl * L4work_E5adder_A3rtl_AP;

		driver_info * L4work_E5adder_D6carryo;
		enumeration * L4work_E5adder_R7addend1,
		            * L4work_E5adder_R7addend2,
		            * L4work_E5adder_R6carryi;
		sig_info<enumeration> * L4work_E5adder_S7addend1,
		                      * L4work_E5adder_S7addend2,
		                      * L4work_E5adder_S6carryi;
		winfo_item winfo[1];
		L4work_E5adder_A3rtl * arch;
};

  /* Implementation of process :work:adder(rtl):_5pn methods */
L4work_E5adder_A3rtl_P4_5pn::
L4work_E5adder_A3rtl_P4_5pn(L4work_E5adder_A3rtl *L4work_E5adder_A3rtl_AP_PAR,name_stack &iname) : process_base(iname) {
    L4work_E5adder_A3rtl_AP = L4work_E5adder_A3rtl_AP_PAR;
    L4work_E5adder_S7addend1 = L4work_E5adder_A3rtl_AP->L4work_E5adder_S7addend1;
    L4work_E5adder_S7addend2 = L4work_E5adder_A3rtl_AP->L4work_E5adder_S7addend2;
    L4work_E5adder_S6carryi = L4work_E5adder_A3rtl_AP->L4work_E5adder_S6carryi;
    L4work_E5adder_R7addend1 = &L4work_E5adder_A3rtl_AP->L4work_E5adder_S7addend1->reader();
    L4work_E5adder_R7addend2 = &L4work_E5adder_A3rtl_AP->L4work_E5adder_S7addend2->reader();
    L4work_E5adder_R6carryi = &L4work_E5adder_A3rtl_AP->L4work_E5adder_S6carryi->reader();
    L4work_E5adder_D6carryo = kernel.get_driver(this,L4work_E5adder_A3rtl_AP->L4work_E5adder_S6carryo);
    {
     sigacl_list sal(3);
     sal.add(L4work_E5adder_A3rtl_AP->L4work_E5adder_S7addend1);
     sal.add(L4work_E5adder_A3rtl_AP->L4work_E5adder_S7addend2);
     sal.add(L4work_E5adder_A3rtl_AP->L4work_E5adder_S6carryi);
     winfo[0] = kernel.setup_wait_info(sal,this);
    }
}
bool L4work_E5adder_A3rtl_P4_5pn::execute() {
    L4work_E5adder_D6carryo->inertial_assign(((((*L4work_E5adder_R7addend1)&&(*L4work_E5adder_R7addend2))||((*L4work_E5adder_R7addend1)&&(*L4work_E5adder_R6carryi)))||((*L4work_E5adder_R7addend2)&&(*L4work_E5adder_R6carryi))),vtime(0));
    return true;
}

/* handle for simulator to find architecture */
void *
L4work_E5adder_A3rtl_handle(name_stack & iname, map_list * mlist,
                            void * father, int level)
{
	REPORT(cout << "Starting constructor L4work_E5adder_A3rtl ..." << endl);
	return new L4work_E5adder_A3rtl(iname, mlist, father, level);
};

extern int L4work_E5adder_A3rtl_init();

handle_info *
L4work_E5adder_A3rtl_hinfo = add_handle("work", "adder", "rtl",
                                        &L4work_E5adder_A3rtl_handle,
                                        &L4work_E5adder_A3rtl_init);

/* Architecture Constructor */
L4work_E5adder_A3rtl::
L4work_E5adder_A3rtl(name_stack & iname, map_list * mlist,
                     void * father, int level)
: L4work_E5adder(iname, mlist, father)
{
	iname.push(":rtl");
	iname.push("");
	kernel.add_process(new L4work_E5adder_A3rtl_P4_4pn(this,iname.set(":_4pn")),":work:adder(rtl)",":_4pn",this);
	kernel.add_process(new L4work_E5adder_A3rtl_P4_5pn(this,iname.set(":_5pn")),":work:adder(rtl)",":_5pn",this);
	iname.pop(); /* pop last declaration from name stack */ iname.pop(); /* pop architecture from name stack */
};

/* Initialization function for architecture :work:adder(rtl) */
int L4work_E5adder_init ();
int L3std_Q8standard_init ();
bool L4work_E5adder_A3rtl_init_done = false;
int L4work_E5adder_A3rtl_init(){
if (L4work_E5adder_A3rtl_init_done) return 1;
L4work_E5adder_A3rtl_init_done=true;
L4work_E5adder_init ();
L3std_Q8standard_init ();
register_source_file("/home/hyronimo/LokaleDaten1/GenodeLabs/vhdl/examples/adder.vhdl","adder.vhdl");
name_stack iname;
iname.push("");
iname.pop();
return 1;
}

/* end of :work:adder(rtl) Architecture */
