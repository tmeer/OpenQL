#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <cassert>

#include <time.h>

#include "src/openql.h"

int main(int argc, char ** argv)
{
    srand(0);

    float sweep_points[]     = {2};  // sizes of the clifford circuits per randomization

    // ql::init(ql::transmon_platform, "instructions.map");

    // create program
    ql::quantum_program prog("prog", 8);
    prog.set_sweep_points(sweep_points, sizeof(sweep_points)/sizeof(float));

    ql::quantum_kernel kernel("kernel8");

    kernel.sdag(6);
    kernel.cphase(2,6);
    kernel.y(3);
    kernel.cnot(7,0);
    kernel.x(0);
    kernel.cphase(7,5);
    kernel.x(4);
    kernel.cphase(0,1);
    kernel.cnot(2,0);
    kernel.cphase(4,1);
    kernel.cnot(4,1);
    kernel.cnot(0,6);
    kernel.cnot(0,3);
    kernel.hadamard(0);
    kernel.hadamard(5);
    kernel.cnot(4,1);
    kernel.hadamard(7);
    kernel.cphase(4,6);
    kernel.hadamard(7);
    kernel.cnot(2,5);
    kernel.cphase(3,1);
    kernel.x(5);
    kernel.cphase(1,4);
    kernel.cnot(4,1);
    kernel.z(1);
    kernel.hadamard(2);
    kernel.hadamard(7);
    kernel.hadamard(5);
    kernel.hadamard(7);
    kernel.cnot(0,7);
    kernel.hadamard(0);
    kernel.x(1);

    prog.add(kernel);
    prog.compile( /*verbose*/ 1 );
    // println(prog.qasm());

    ql::quantum_program sprog = prog;
    sprog.schedule();
    // println(sprog.qasm());

    return 0;
}
