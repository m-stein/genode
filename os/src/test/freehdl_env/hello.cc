#include <freehdl/kernel.h>
#include <freehdl/std.h>


/* External declarations */
/* End of external declarations */
/* Entity class declaration */
class L4work_E11hello_world {
public:
   void *father_component;
  L4work_E11hello_world (name_stack &iname, map_list *mlist, void *father);
  ~L4work_E11hello_world() {};
};

/* Implementation of entity methods */
L4work_E11hello_world::
L4work_E11hello_world(name_stack &iname, map_list *mlist, void *father) {
    father_component = father;
    iname.push("");
    iname.pop();
};


/* Initialization function for entity :work:hello_world */
int L3std_Q8standard_init ();
int L3std_Q6textio_init ();
bool L4work_E11hello_world_init_done = false;
int L4work_E11hello_world_init(){
if (L4work_E11hello_world_init_done) return 1;
L4work_E11hello_world_init_done=true;
L3std_Q8standard_init ();
L3std_Q6textio_init ();
register_source_file("/home/hyronimo/LokaleDaten1/GenodeLabs/vhdl/examples/hello.vhdl","hello.vhdl");
name_stack iname;
iname.push("");
iname.pop();
return 1;
}

/* end of L4work_E11hello_world Entity */
/* External declarations */
/* Definitions for file type :std:textio:text */
#define L3std_Q6textio_I4text vhdlfile_info_base
extern vhdlfile_info_base L3std_Q6textio_I4text_INFO;
/* Definitions for access type :std:textio:line */
#define L3std_Q6textio_I4line access_info_base
extern access_info_base L3std_Q6textio_I4line_INFO;
/* Prototype for subprogram :std:textio:writeline */
void L3std_Q6textio_X9writeline_i90(vhdlfile &,vhdlaccess &);
extern vhdlfile L3std_Q6textio_V6output;
/* Definitions for enumeration type :std:textio:side */
class L3std_Q6textio_I4side:public enum_info_base{
   static const char *values[];
public:
   L3std_Q6textio_I4side():enum_info_base(0,1,values) {};
   static const char **get_values() { return values; }
   static int low() { return 0; }
   static int high() { return 1; }
   static int left() { return 0; }
   static int right() { return 1; }
};
extern L3std_Q6textio_I4side L3std_Q6textio_I4side_INFO;
/* Prototype for subprogram :std:textio:write */
void L3std_Q6textio_X5write_i126(vhdlaccess &,const L3std_Q8standard_T6string &,const enumeration ,const integer );
/* End of external declarations */
/* Architecture class declaration */
class L4work_E11hello_world_A9behaviour : public L4work_E11hello_world {
public:
  L4work_E11hello_world_A9behaviour (name_stack &iname, map_list *mlist, void *father, int level);
  ~L4work_E11hello_world_A9behaviour();
};

/* Concurrent statement class declaration(s) */


/* Class decl. process _4pn */
class L4work_E11hello_world_A9behaviour_P4_4pn : public process_base {
public:
  L4work_E11hello_world_A9behaviour_P4_4pn(L4work_E11hello_world_A9behaviour* L4work_E11hello_world_A9behaviour_AP_PAR,name_stack &iname);
  ~L4work_E11hello_world_A9behaviour_P4_4pn() {};
  bool execute();
  L4work_E11hello_world_A9behaviour* L4work_E11hello_world_A9behaviour_AP;
  vhdlaccess L4work_E11hello_world_A9behaviour_P4_4pn_V1l;
  L4work_E11hello_world_A9behaviour *arch;
};
enumeration L4work_E11hello_world_A9behaviour_itn0_lit[]={72,101,108,108,111,32,119,111,114,108,100,33};
  /* Implementation of process :work:hello_world(behaviour):_4pn methods */
L4work_E11hello_world_A9behaviour_P4_4pn::
L4work_E11hello_world_A9behaviour_P4_4pn(L4work_E11hello_world_A9behaviour *L4work_E11hello_world_A9behaviour_AP_PAR,name_stack &iname) : process_base(iname) {
    L4work_E11hello_world_A9behaviour_AP=L4work_E11hello_world_A9behaviour_AP_PAR;
    L4work_E11hello_world_A9behaviour_P4_4pn_V1l=NULL;
}
bool L4work_E11hello_world_A9behaviour_P4_4pn::execute() {
    L3std_Q6textio_X5write_i126 (L4work_E11hello_world_A9behaviour_P4_4pn_V1l,array_alias<L3std_Q8standard_T6string >(new array_info((&L3std_Q8standard_I6string_INFO)->element_type,(&L3std_Q8standard_I6string_INFO)->index_type,1,to,1+11,0),L4work_E11hello_world_A9behaviour_itn0_lit),enumeration(0),0);
    L3std_Q6textio_X9writeline_i90 (L3std_Q6textio_V6output,L4work_E11hello_world_A9behaviour_P4_4pn_V1l);
    wait(PROCESS_STOP); return true;
    return true;
}

/* handle for simulator to find architecture */
void*
L4work_E11hello_world_A9behaviour_handle(name_stack &iname, map_list *mlist, void *father, int level) {
 REPORT(cout << "Starting constructor L4work_E11hello_world_A9behaviour ..." << endl);
 return new L4work_E11hello_world_A9behaviour(iname, mlist, father, level);
};
extern int L4work_E11hello_world_A9behaviour_init ();
handle_info *L4work_E11hello_world_A9behaviour_hinfo =
  add_handle("work","hello_world","behaviour",&L4work_E11hello_world_A9behaviour_handle,&L4work_E11hello_world_A9behaviour_init);
/* Architecture Constructor */
L4work_E11hello_world_A9behaviour::
L4work_E11hello_world_A9behaviour(name_stack &iname, map_list *mlist, void *father, int level) :
  L4work_E11hello_world(iname, mlist, father) {
    iname.push(":behaviour");
    iname.push("");
    kernel.add_process(new L4work_E11hello_world_A9behaviour_P4_4pn(this,iname.set(":_4pn")),":work:hello_world(behaviour)",":_4pn",this);
    iname.pop(); /* pop last declaration from name stack */ iname.pop(); /* pop architecture from name stack */
};

/* Initialization function for architecture :work:hello_world(behaviour) */
int L4work_E11hello_world_init ();
int L3std_Q8standard_init ();
bool L4work_E11hello_world_A9behaviour_init_done = false;
int L4work_E11hello_world_A9behaviour_init(){
if (L4work_E11hello_world_A9behaviour_init_done) return 1;
L4work_E11hello_world_A9behaviour_init_done=true;
L4work_E11hello_world_init ();
L3std_Q8standard_init ();
register_source_file("/home/hyronimo/LokaleDaten1/GenodeLabs/vhdl/examples/hello.vhdl","hello.vhdl");
name_stack iname;
iname.push("");
iname.pop();
return 1;
}

/* end of :work:hello_world(behaviour) Architecture */
