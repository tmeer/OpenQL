#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <cassert>

#include <time.h>

#include <ql/openql.h>

// steane qec on s7 with cnots
void
test_steaneqec(std::string v, std::string schedopt, std::string sched_post179opt)
{
    int n = 7;
    std::string prog_name = "test_" + v + "_schedopt=" + schedopt + "_sched_post179opt=" + sched_post179opt;
    std::string kernel_name = "test_" + v + "_schedopt=" + schedopt + "_sched_post179opt=" + sched_post179opt;
    float sweep_points[] = { 1 };

    ql::quantum_platform starmon("starmon", "test_ideal.json");
    ql::set_platform(starmon);
    ql::quantum_program prog(prog_name, starmon, n, 0);
    ql::quantum_kernel k(kernel_name, starmon, n, 0);
    prog.set_sweep_points(sweep_points, sizeof(sweep_points)/sizeof(float));

    k.gate("prepz", 3);
    k.gate("prepz", 5);
    k.gate("h", 5);
    k.gate("cnot", 5, 3);
    k.gate("cnot", 0, 3);
    k.gate("cnot", 1, 3);
    k.gate("cnot", 6, 3);
    k.gate("cnot", 2, 5);
    k.gate("cnot", 5, 3);
    k.gate("h", 5);
    k.gate("measure", 3);
    k.gate("measure", 5);

    prog.add(k);
    
    ql::options::set("scheduler", schedopt);
    ql::options::set("scheduler_post179", sched_post179opt);
    prog.compile( );
}

// all cnots with operands that are neighbors in s7
// no or hardly any significant difference between pre179 and post179 scheduling,
// slight differences may occur when the json file maps cnot to its constituent primitive gates
void
test_manyNN(std::string v, std::string schedopt, std::string sched_post179opt)
{
    int n = 7;
    std::string prog_name = "test_" + v + "_schedopt=" + schedopt + "_sched_post179opt=" + sched_post179opt;
    std::string kernel_name = "test_" + v + "_schedopt=" + schedopt + "_sched_post179opt=" + sched_post179opt;
    float sweep_points[] = { 1 };

    ql::quantum_platform starmon("starmon", "test_ideal.json");
    ql::set_platform(starmon);
    ql::quantum_program prog(prog_name, starmon, n, 0);
    ql::quantum_kernel k(kernel_name, starmon, n, 0);
    prog.set_sweep_points(sweep_points, sizeof(sweep_points)/sizeof(float));

    for (int j=0; j<7; j++) { k.gate("x", j); }

    // a list of all cnots that are ok in trivial mapping
    k.gate("cnot", 0,2);
    k.gate("cnot", 0,3);
    k.gate("cnot", 1,3);
    k.gate("cnot", 1,4);
    k.gate("cnot", 2,0);
    k.gate("cnot", 2,5);
    k.gate("cnot", 3,0);
    k.gate("cnot", 3,1);
    k.gate("cnot", 3,5);
    k.gate("cnot", 3,6);
    k.gate("cnot", 4,1);
    k.gate("cnot", 4,6);
    k.gate("cnot", 5,2);
    k.gate("cnot", 5,3);
    k.gate("cnot", 6,3);
    k.gate("cnot", 6,4);

    for (int j=0; j<7; j++) { k.gate("x", j); }

    prog.add(k);

    ql::options::set("scheduler", schedopt);
    ql::options::set("scheduler_post179", sched_post179opt);
    prog.compile( );
}

// test cnot control operand commutativity
// i.e. best result is the reverse original order
void
test_cnot_controlcommute(std::string v, std::string schedopt, std::string sched_post179opt)
{
    int n = 7;
    std::string prog_name = "test_" + v + "_schedopt=" + schedopt + "_sched_post179opt=" + sched_post179opt;
    std::string kernel_name = "test_" + v + "_schedopt=" + schedopt + "_sched_post179opt=" + sched_post179opt;
    float sweep_points[] = { 1 };

    ql::quantum_platform starmon("starmon", "test_ideal.json");
    ql::set_platform(starmon);
    ql::quantum_program prog(prog_name, starmon, n, 0);
    ql::quantum_kernel k(kernel_name, starmon, n, 0);
    prog.set_sweep_points(sweep_points, sizeof(sweep_points)/sizeof(float));

    // for (int j=0; j<7; j++) { k.gate("x", j); }

    k.gate("cnot", 3,0);
    k.gate("cnot", 3,6);
    k.gate("t", 6);
    k.gate("y", 6);
    k.gate("cnot", 3,1);
    k.gate("t", 1);
    k.gate("y", 1);
    k.gate("t", 1);
    k.gate("y", 1);
    k.gate("cnot", 3,5);
    k.gate("t", 5);
    k.gate("y", 5);
    k.gate("t", 5);
    k.gate("y", 5);
    k.gate("t", 5);
    k.gate("y", 5);

    // for (int j=0; j<7; j++) { k.gate("x", j); }

    prog.add(k);

    ql::options::set("scheduler", schedopt);
    ql::options::set("scheduler_post179", sched_post179opt);
    prog.compile( );
}

// test cnot target operand commutativity
// i.e. best result is the reverse original order
void
test_cnot_targetcommute(std::string v, std::string schedopt, std::string sched_post179opt)
{
    int n = 7;
    std::string prog_name = "test_" + v + "_schedopt=" + schedopt + "_sched_post179opt=" + sched_post179opt;
    std::string kernel_name = "test_" + v + "_schedopt=" + schedopt + "_sched_post179opt=" + sched_post179opt;
    float sweep_points[] = { 1 };

    ql::quantum_platform starmon("starmon", "test_ideal.json");
    ql::set_platform(starmon);
    ql::quantum_program prog(prog_name, starmon, n, 0);
    ql::quantum_kernel k(kernel_name, starmon, n, 0);
    prog.set_sweep_points(sweep_points, sizeof(sweep_points)/sizeof(float));

    // for (int j=0; j<7; j++) { k.gate("x", j); }

    k.gate("cnot", 0,3);
    k.gate("cnot", 6,3);
    k.gate("t", 6);
    k.gate("y", 6);
    k.gate("cnot", 1,3);
    k.gate("t", 1);
    k.gate("y", 1);
    k.gate("t", 1);
    k.gate("y", 1);
    k.gate("cnot", 5,3);
    k.gate("t", 5);
    k.gate("y", 5);
    k.gate("t", 5);
    k.gate("y", 5);
    k.gate("t", 5);
    k.gate("y", 5);

    // for (int j=0; j<7; j++) { k.gate("x", j); }

    prog.add(k);

    ql::options::set("scheduler", schedopt);
    ql::options::set("scheduler_post179", sched_post179opt);
    prog.compile( );
}

// test cz any operand commutativity
// i.e. best result is the reverse original order
void
test_cz_anycommute(std::string v, std::string schedopt, std::string sched_post179opt)
{
    int n = 7;
    std::string prog_name = "test_" + v + "_schedopt=" + schedopt + "_sched_post179opt=" + sched_post179opt;
    std::string kernel_name = "test_" + v + "_schedopt=" + schedopt + "_sched_post179opt=" + sched_post179opt;
    float sweep_points[] = { 1 };

    ql::quantum_platform starmon("starmon", "test_ideal.json");
    ql::set_platform(starmon);
    ql::quantum_program prog(prog_name, starmon, n, 0);
    ql::quantum_kernel k(kernel_name, starmon, n, 0);
    prog.set_sweep_points(sweep_points, sizeof(sweep_points)/sizeof(float));

    // for (int j=0; j<7; j++) { k.gate("x", j); }

    k.gate("cz", 0,3);
    k.gate("cz", 3,6);
    k.gate("t", 6);
    k.gate("y", 6);
    k.gate("cz", 1,3);
    k.gate("t", 1);
    k.gate("y", 1);
    k.gate("t", 1);
    k.gate("y", 1);
    k.gate("cz", 3,5);
    k.gate("t", 5);
    k.gate("y", 5);
    k.gate("t", 5);
    k.gate("y", 5);
    k.gate("t", 5);
    k.gate("y", 5);

    // for (int j=0; j<7; j++) { k.gate("x", j); }

    prog.add(k);

    ql::options::set("scheduler", schedopt);
    ql::options::set("scheduler_post179", sched_post179opt);
    prog.compile( );
}

int main(int argc, char ** argv)
{
    ql::utils::logger::set_log_level("LOG_DEBUG");
    ql::options::set("scheduler_uniform", "no");

//  test_cnot_controlcommute("cnot_controlcommute", "ASAP", "no");
//  test_cnot_controlcommute("cnot_controlcommute", "ASAP", "yes");
//  test_cnot_controlcommute("cnot_controlcommute", "ALAP", "no");
//  test_cnot_controlcommute("cnot_controlcommute", "ALAP", "yes");
//  test_cnot_targetcommute("cnot_targetcommute", "ASAP", "no");
//  test_cnot_targetcommute("cnot_targetcommute", "ASAP", "yes");
//  test_cnot_targetcommute("cnot_targetcommute", "ALAP", "no");
//  test_cnot_targetcommute("cnot_targetcommute", "ALAP", "yes");
//  test_cz_anycommute("cz_anycommute", "ASAP", "no");
//  test_cz_anycommute("cz_anycommute", "ASAP", "yes");
//  test_cz_anycommute("cz_anycommute", "ALAP", "no");
//  test_cz_anycommute("cz_anycommute", "ALAP", "yes");
//  test_steaneqec("steaneqec", "ASAP", "no");
//  test_steaneqec("steaneqec", "ASAP", "yes");
//  test_steaneqec("steaneqec", "ALAP", "no");
    test_steaneqec("steaneqec", "ALAP", "yes");
//  test_manyNN("manyNN", "ASAP", "no");
//  test_manyNN("manyNN", "ASAP", "yes");
//  test_manyNN("manyNN", "ALAP", "no");
//  test_manyNN("manyNN", "ALAP", "yes");

//  ql::options::set("scheduler_uniform", "yes");
//  test_hilo("hilo_uniform", "ALAP", "no");
//  test_hilo("hilo_uniform", "ALAP", "yes");

    return 0;
}
