/* Main function for architecture :work:hello_world(behaviour) */
#include<freehdl/kernel-handle.hh>
int main (int argc, char *argv[]) 

{
  extern handle_info *L4work_E11hello_world_A9behaviour_hinfo;
  extern int kernel_main (int, char *[], handle_info*);
  return kernel_main (argc, argv, L4work_E11hello_world_A9behaviour_hinfo);
}

/* end of :work:hello_world(behaviour) main function */
