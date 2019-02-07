/**
 * @file   ideal_eqasm_compiler.h
 * @date   02/2019
 * @author Hans van Someren
 * @brief  ideal compiler implementation
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

class Depgraph : public Scheduler
{
public:

    void show_deps()
    {
        InDegMap<ListDigraph> inDeg(graph);
        for (ListDigraph::NodeIt n(graph); n != INVALID; ++n)
        {
            DOUT("Incoming unfiltered dependences of node " << ": " << instruction[n]->qasm() << " :");
            std::list<ListDigraph::Arc> Rarclist;
            std::list<ListDigraph::Arc> Darclist;
            for( ListDigraph::InArcIt arc(graph,n); arc != INVALID; ++arc )
            {
                ListDigraph::Node srcNode  = graph.source(arc);
                if (depType[arc] == WAW
                ||  depType[arc] == RAW
                ||  depType[arc] == DAW
                ) {
                    continue;
                }
                DOUT("... Encountering relevant " << DepTypesNames[depType[arc]] << " by q" << cause[arc] << " from " << instruction[srcNode]->qasm());
                if (depType[arc] == WAR
                ||  depType[arc] == DAR
                ) {
                    Rarclist.push_back(arc);
                }
                else
                if (depType[arc] == WAD
                ||  depType[arc] == RAD
                ) {
                    Darclist.push_back(arc);
                }
            }
            while (Rarclist.size() > 1)
            {
                std::list<ListDigraph::Arc> TMParclist = Rarclist;
                int  operand = cause[Rarclist.front()];
                TMParclist.remove_if([this,operand](ListDigraph::Arc a) { return cause[a] != operand; });
                if (TMParclist.size() > 1)
                {
                    DOUT("At " << instruction[n]->qasm() << " found commuting gates on q" << operand << ":");
                    for ( auto a : TMParclist )
                    {
                        ListDigraph::Node srcNode  = graph.source(a);
                        DOUT("... " << instruction[srcNode]->qasm() << " as " << DepTypesNames[depType[a]] << " by q" << cause[a]);
                    }
                }
                Rarclist.remove_if([this,operand](ListDigraph::Arc a) { return cause[a] == operand; });
            }
            while (Darclist.size() > 1)
            {
                std::list<ListDigraph::Arc> TMParclist = Darclist;
                int  operand = cause[Darclist.front()];
                TMParclist.remove_if([this,operand](ListDigraph::Arc a) { return cause[a] != operand; });
                if (TMParclist.size() > 1)
                {
                    DOUT("At " << instruction[n]->qasm() << " found commuting gates on q" << operand << ":");
                    for ( auto a : TMParclist )
                    {
                        ListDigraph::Node srcNode  = graph.source(a);
                        DOUT("... " << instruction[srcNode]->qasm() << " as " << DepTypesNames[depType[a]] << " by q" << cause[a]);
                    }
                }
                Darclist.remove_if([this,operand](ListDigraph::Arc a) { return cause[a] == operand; });
            }
        }
    }

    void do_schedule(resource_manager_t& rm, const ql::quantum_platform& platform)
    {
	    std::string schedopt = ql::options::get("scheduler");
	    if ("ASAP" == schedopt)
	    {
	        schedule_asap(rm, platform);
	    }
	    else if ("ALAP" == schedopt)
	    {
	        schedule_alap(rm, platform);
	    }
	    else
	    {
	        EOUT("Unknown scheduler");
	        throw ql::exception("Unknown scheduler!", false);
	    }
    }
};

/**
 * ideal eqasm compiler
 */
class ideal_eqasm_compiler : public eqasm_compiler
{
public:

    size_t          num_qubits;
    size_t          cycle_time;

private:

    void print(quantum_kernel& kernel, size_t perm)
    {
	    std::stringstream ss_output_file;
	    ss_output_file << ql::options::get("output_dir") << "/" << kernel.name << "_" << perm << ".qasm";
	    DOUT("writing permutation to '" << ss_output_file.str() << "' ...");
	    std::stringstream ss_qasm;
	    ss_qasm << "." << kernel.name << "_" << perm << '\n';
	    ql::circuit& ckt = kernel.c;
	    for (auto gp : ckt)
	    {
	        ss_qasm << '\t' << gp->qasm();
	        // ss_qasm << "\t# " << gp->cycle
	        ss_qasm << '\n';
	    }
	    size_t  depth = ckt.back()->cycle + std::ceil( static_cast<float>(ckt.back()->duration) / cycle_time) - ckt.front()->cycle;
	    ss_qasm <<  "# depth=" << depth;
	    ql::utils::write_file(ss_output_file.str(), ss_qasm.str());
	}

    scheduling_direction_t  get_direction()
    {
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
        return direction;
    }

	void generate_permutations(quantum_kernel& kernel,
	    const ql::quantum_platform & platform, size_t nqubits, size_t ncreg = 0)
	{
	    DOUT("Generate commutable variations of kernel circuit ...");
	    ql::circuit& ckt = kernel.c;
	    if (ckt.empty())
	    {
	        DOUT("Empty kernel " << kernel.name);
	        return;
	    }

	    resource_manager_t rm(platform, get_direction());
	    Depgraph sched;
	    sched.Init(ckt, platform, nqubits, ncreg);
	
	    // have a dependence graph
        sched.show_deps();

	    // find the commutable sets of nodes
	    // convert each to permutations and combine those
	    int perm = 0;
	    // for each permutation:
	    //  - generate additional dependences
	    //  - check dag, if not, continue
	    //  - schedule
	    sched.do_schedule(rm, platform);
	    //  - print
	    print(kernel, perm);
	    //  - remove additional dependences
        //  ...
	    
	    DOUT("Generate commutable variations of kernel circuit [Done]");
    }

    // kernel level compilation
    void compile(std::string prog_name, std::vector<quantum_kernel> kernels, const ql::quantum_platform& platform)
    {
        DOUT("Compiling " << kernels.size() << " kernels to generate commuting permutations ... ");

        load_hw_settings(platform);

        for(auto &kernel : kernels)
        {
            IOUT("Compiling kernel: " << kernel.name);
            auto num_creg = kernel.creg_count;
            generate_permutations(kernel, platform, num_qubits, num_creg);
        }

        DOUT("Compiling Ideal eQASM [Done]");
        DOUT("============================");
        DOUT("");
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
            cycle_time                                    = platform.hardware_settings[params[p++]];
        }
        catch (json::exception e)
        {
            throw ql::exception("[x] error : ql::eqasm_compiler::compile() : error while reading hardware settings : parameter '"+params[p-1]+"'\n\t"+ std::string(e.what()),false);
        }
    }

public:
    /*
     * program-level compilation of qasm to ideal_eqasm
     */
    void compile(std::string prog_name, ql::circuit& ckt, ql::quantum_platform& platform)
    {
        EOUT("deprecated compile interface");
        throw ql::exception("deprecated compile interface", false);
    }


};
}
}

#endif // QL_IDEAL_EQASM_COMPILER_H

