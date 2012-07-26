
/* Main function for architecture :work:adder(rtl) */
#include<freehdl/kernel-handle.hh>

void start_emulation_frontend();

/**
 * Main routine
 */
int main (int argc, char * argv[])
{
	/* start Genode emulation-frontend */
	start_emulation_frontend();

	extern handle_info * L4work_E5adder_A3rtl_hinfo;
	extern int kernel_main (int, char *[], handle_info*);
	return kernel_main (argc, argv, L4work_E5adder_A3rtl_hinfo);
}

/* end of :work:adder(rtl) main function */
