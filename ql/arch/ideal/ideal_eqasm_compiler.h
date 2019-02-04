/**
 * @file   ideal_eqasm_compiler.h
 * @date   08/2017
 * @author Imran Ashraf
 *         Nader Khammassi
 * @brief  cclighteqasm compiler implementation
 */

#ifndef QL_IDEAL_EQASM_COMPILER_H
#define QL_IDEAL_EQASM_COMPILER_H

#include <ql/utils.h>
#include <ql/platform.h>
#include <ql/kernel.h>
#include <ql/gate.h>
#include <ql/ir.h>
#include <ql/eqasm_compiler.h>
#include <ql/scheduler.h>
#include <ql/arch/cc_light/cc_light_resource_manager.h>

namespace ql
{
namespace arch
{

ql::ir::bundles_t ideal_schedule_rc(ql::circuit & ckt,
    const ql::quantum_platform & platform, size_t nqubits, size_t ncreg = 0)
{
    IOUT("Resource constraint scheduling of ideal instructions ...");
    scheduling_direction_t  direction;
    std::string schedopt = ql::options::get("scheduler");
    if ("ASAP" == schedopt)
    {
        direction = forward_scheduling;
    }
    else if ("ALAP" == schedopt)
    {
        direction = backward_scheduling;
    }
    else
    {
        EOUT("Unknown scheduler");
        throw ql::exception("Unknown scheduler!", false);

    }
    resource_manager_t rm(platform, direction);

    Scheduler sched;
    sched.Init(ckt, platform, nqubits, ncreg);
    ql::ir::bundles_t bundles;
    if ("ASAP" == schedopt)
    {
        bundles = sched.schedule_asap(rm, platform);
    }
    else if ("ALAP" == schedopt)
    {
        bundles = sched.schedule_alap(rm, platform);
    }
    else
    {
        EOUT("Unknown scheduler");
        throw ql::exception("Unknown scheduler!", false);

    }

    DOUT("have yet to print bundles");

    IOUT("Resource constraint scheduling of ideal instructions [Done].");
    return bundles;
}

/**
 * ideal eqasm compiler
 */
class ideal_eqasm_compiler : public eqasm_compiler
{
public:

    size_t          num_qubits;
    size_t          ns_per_cycle;
    size_t          total_exec_time = 0;

#define __ns_to_cycle(t) ((size_t)t/(size_t)ns_per_cycle)

public:


    /*
     * program-level compilation of qasm to ideal_eqasm
     */
    void compile(std::string prog_name, ql::circuit& ckt, ql::quantum_platform& platform)
    {
        EOUT("deprecated compile interface");
        throw ql::exception("deprecated compile interface", false);
    }


    // kernel level compilation
    void compile(std::string prog_name, std::vector<quantum_kernel> kernels, const ql::quantum_platform& platform)
    {
        DOUT("Compiling " << kernels.size() << " kernels to generate CCLight eQASM ... ");

        load_hw_settings(platform);

        for(auto &kernel : kernels)
        {
            IOUT("Compiling kernel: " << kernel.name);
            ql::circuit& ckt = kernel.c;
            auto num_creg = kernel.creg_count;
            if (! ckt.empty())
            {
                // schedule with platform resource constraints
                ql::ir::bundles_t bundles = ideal_schedule_rc(ckt, platform, num_qubits, num_creg);
            }
        }

        DOUT("Compiling Ideal eQASM [Done]");
    }


private:

    void load_hw_settings(const ql::quantum_platform& platform)
    {
        std::string params[] = { "qubit_number", "cycle_time"
                               };
        size_t p = 0;

        DOUT("Loading hardware settings ...");
        try
        {
            num_qubits                                      = platform.hardware_settings[params[p++]];
            ns_per_cycle                                    = platform.hardware_settings[params[p++]];
        }
        catch (json::exception e)
        {
            throw ql::exception("[x] error : ql::eqasm_compiler::compile() : error while reading hardware settings : parameter '"+params[p-1]+"'\n\t"+ std::string(e.what()),false);
        }
    }

    /**
     * emit qasm code
     */
    void emit_eqasm(bool verbose=false)
    {
        IOUT("emitting eqasm...");
        eqasm_code.clear();
        IOUT("emitting eqasm code done.");
    }

};
}
}

#endif // QL_IDEAL_EQASM_COMPILER_H

