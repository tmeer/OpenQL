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
    void clean_variation( std::list<ListDigraph::Arc>& newarcslist)
    {
        for ( auto a : newarcslist)
        {
            auto srcNode = graph.source(a);
            auto tgtNode = graph.target(a);
            DOUT("...... erasing arc from " << instruction[srcNode]->qasm() << " to " << instruction[tgtNode]->qasm() << " as " << DepTypesNames[depType[a]] << " by q" << cause[a]);
            graph.erase(a);
        }
    }

    void gen_variation(std::list<std::list<ListDigraph::Arc>>& varslist, std::list<ListDigraph::Arc>& newarcslist, int var)
    {
        std::list<std::list<ListDigraph::Arc>> recipe_varslist = varslist;  // deepcopy, i.e. must copy each sublist of list as well
        DOUT("... variation " << var << ":");
        // DOUT("... recipe_varslist.size()=" << recipe_varslist.size());
        int list_index = 1;
        for ( auto subvarslist : recipe_varslist )
        {
            // DOUT("... subvarslist index=" << list_index << " subvarslist.size()=" << subvarslist.size());
            bool prevvalid = false;             // add arc between each pair of nodes so skip 1st arc in subvarslist
            ListDigraph::Node    prevn = s;     // previous node when prevvalid==true; fake initialization by s
            for (auto i = subvarslist.size(); i != 0; i--)
            {
                auto thisone = var % i;     // gives 0 <= thisone < subvarslist.size()
                // DOUT("...... var=" << var << " i=" << i << " thisone=var%i=" << thisone << " nextvar=var/i=" << var/i);
                auto li = subvarslist.begin();
                std::advance(li, thisone);
                ListDigraph::Arc a = *li;
                ListDigraph::Node n  = graph.source(a);
                DOUT("...... list " << list_index << " sub " << thisone << ": " << instruction[n]->qasm() << " as " << DepTypesNames[depType[a]] << " by q" << cause[a]);
                if (prevvalid)
                {
                    // DOUT("...... adding arc from " << instruction[prevn]->qasm() << " to " << instruction[n]->qasm());
                    auto newarc = graph.addArc(prevn, n);
                    weight[newarc] = weight[a];
                    cause[newarc] = cause[a];
                    depType[newarc] = (depType[a] == WAR ? RAR : DAD);
                    DOUT("...... added arc from " << instruction[prevn]->qasm() << " to " << instruction[n]->qasm() << " as " << DepTypesNames[depType[newarc]] << " by q" << cause[newarc]);
                    newarcslist.push_back(newarc);
                }
                prevvalid = true;
                prevn = n;
                subvarslist.erase(li);
                var = var / i;
            }
            list_index++;
        }
    }

    void add_variations(std::list<ListDigraph::Arc>& arclist, std::list<std::list<ListDigraph::Arc>>& varslist, int& var_count)
    {
        while (arclist.size() > 1)
        {
            std::list<ListDigraph::Arc> TMParclist = arclist;
            int  operand = cause[arclist.front()];
            TMParclist.remove_if([this,operand](ListDigraph::Arc a) { return cause[a] != operand; });
            if (TMParclist.size() > 1)
            {
                // DOUT("At " << instruction[graph.target(TMParclist.front())]->qasm() << " found commuting gates on q" << operand << ":");
                int perm_index = 0;
                int perm_count = 1;
                std::list<ListDigraph::Arc> var;
                for ( auto a : TMParclist )
                {
                    ListDigraph::Node srcNode  = graph.source(a);
                    // DOUT("... " << instruction[srcNode]->qasm() << " as " << DepTypesNames[depType[a]] << " by q" << cause[a]);
                    perm_index++;
                    perm_count *= perm_index;
                    var.push_back(a);
                }
                varslist.push_back(var);
                var_count *= perm_count;
            }
            arclist.remove_if([this,operand](ListDigraph::Arc a) { return cause[a] == operand; });
        }
    }

    void print_variations(std::list<std::list<ListDigraph::Arc>>& varslist)
    {
        int var_count = 1;
        for ( auto subvarslist : varslist )
        {
            DOUT("Commuting set:");
            int perm_index = 0;
            int perm_count = 1;
            for (auto a : subvarslist)
            {
                ListDigraph::Node srcNode  = graph.source(a);
                DOUT("... " << instruction[srcNode]->qasm() << " as " << DepTypesNames[depType[a]] << " by q" << cause[a]);
                perm_index++;
                perm_count *= perm_index;
            }
            DOUT("Giving rise to " << perm_count << " variations");
            var_count *= perm_count;
        }
        DOUT("Total " << var_count << " variations");
    }

    void find_variations(std::list<std::list<ListDigraph::Arc>>& varslist, int& total)
    {
        InDegMap<ListDigraph> inDeg(graph);
        for (ListDigraph::NodeIt n(graph); n != INVALID; ++n)
        {
            // DOUT("Incoming unfiltered dependences of node " << ": " << instruction[n]->qasm() << " :");
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
                // DOUT("... Encountering relevant " << DepTypesNames[depType[arc]] << " by q" << cause[arc] << " from " << instruction[srcNode]->qasm());
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
            add_variations(Rarclist, varslist, total);
            add_variations(Darclist, varslist, total);
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
	    DOUT("... writing variation to '" << ss_output_file.str() << "' ...");
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
	    ss_qasm <<  "# Depth=" << depth << '\n';
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

	void generate_variations(quantum_kernel& kernel,
	    const ql::quantum_platform & platform, size_t nqubits, size_t ncreg = 0)
	{
	    DOUT("Generate commutable variations of kernel circuit ...");
	    ql::circuit& ckt = kernel.c;
	    if (ckt.empty())
	    {
	        DOUT("Empty kernel " << kernel.name);
	        return;
	    }

	    Depgraph sched;
	    sched.Init(ckt, platform, nqubits, ncreg);
	
	    // find the sets of sets of commutable nodes and store these in the varslist
        //
        // each set of commutable node in principle gives rise to a full set of permutations (variations)
        // multiple sets of such give rise to the multiplication of those permutations
        // the total number of such variations is computed as well to prepare for a goedelization of the variations
        //
        // varslist is implemented as a list of lists of arcs from gates that commute
        // arcs instead of nodes are stored because arc also contains deptype[] and cause[] next to its source node
        DOUT("Finding sets of commutable gates ...");
        std::list<std::list<ListDigraph::Arc>> varslist;
        int total = 1;
        sched.find_variations(varslist, total);
        sched.print_variations(varslist);

        DOUT("Start generating " << total << " variations ...");
        DOUT("=========================\n\n");
        for (int perm = 0; perm < total; perm++)
        {
            std::list<ListDigraph::Arc> newarcslist;        // new deps generated
	        // generate additional dependences for this variation
            sched.gen_variation(varslist, newarcslist, perm);
            if ( !dag(sched.graph))
            {
                // there are cycles among the dependences so this variation is infeasible
                DOUT("... variation " << perm << " results in a dependence cycle, skip it");
            }
            else
            {
	            DOUT("... schedule variation " << perm);
		        resource_manager_t rm(platform, get_direction());
		        sched.do_schedule(rm, platform);
	            DOUT("... generating qasm code for this variation " << perm);
		        print(kernel, perm);
            }
            sched.clean_variation(newarcslist);
            DOUT("... ready with variation " << perm);
            DOUT("=========================\n");
        }
	    
	    DOUT("Generate commutable variations of kernel circuit [Done]");
    }

    // kernel level compilation
    void compile(std::string prog_name, std::vector<quantum_kernel> kernels, const ql::quantum_platform& platform)
    {
        DOUT("Compiling " << kernels.size() << " kernels to generate commuting variations ... ");

        load_hw_settings(platform);

        for(auto &kernel : kernels)
        {
            IOUT("Compiling kernel: " << kernel.name);
            auto num_creg = kernel.creg_count;
            generate_variations(kernel, platform, num_qubits, num_creg);
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

