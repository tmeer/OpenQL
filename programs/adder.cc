

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <cassert>

#include <time.h>

#include <ql/openql.h>


int main(int argc, char ** argv)
{
   srand(argc);

   size_t   num_circuits       = 4;
   size_t   num_qubits         = 4;
   float    sweep_points[]   = { 2, 4, 8, 16 };  

   // openql runtime options
   ql::options::set("log_level", "LOG_NOTHING");
   ql::options::set("output_dir", "output");
   ql::options::set("optimize", "yes");
   ql::options::set("scheduler", "ASAP");
   ql::options::set("use_default_gates", "yes");
   ql::options::set("decompose_toffoli", "NC");

   // create platform
   ql::quantum_platform starmon("starmon","test_cfg_cbox.json");

   // print info
   starmon.print_info();

   // set platform
   ql::set_platform(starmon);

   // ql::sweep_points_t sweep_points;
   
   // create program
   ql::quantum_program adder("adder",starmon,num_qubits,0);

   // set sweep points
   adder.set_sweep_points(sweep_points, num_circuits);
   
   // create kernels
   ql::quantum_kernel init("init",starmon,num_qubits,0);
   ql::quantum_kernel add("add",starmon,num_qubits,0);

   // buid init
   init.prepz(0);
   init.x(0);

   // build add
   add.toffoli(0,1,2);
   add.cnot(0,1);
   add.measure(0);
   add.measure(1);
   add.measure(2);


   // build rb
   // build_rb(4, kernel, 0);
   // build_rb(8, kernel, 1);

   // kernel.loop(10);
   adder.add(init);
   adder.add(add);

   // std::cout<< rb.qasm() << std::endl;

   adder.compile();

   // std::cout << rb.qasm() << std::endl;

   return 0;
}
