/**
 * @file   commute_variation.h
 * @date   02/2019
 * @author Hans van Someren
 * @brief  find circuit variations from commutable sets of gates and select shortest
 */

#ifndef QL_COMMUTE_VARIATION_H
#define QL_COMMUTE_VARIATION_H

/*
    Summary

    Commutation of gates such as Control-Unitaries (CZ, CNOT, etc.) is exploited
    to find all variations of a given circuit by varying on the order of those commutations.
    Each of the variations can be printed to a separate file.
    At the end, the current kernel's circuit is replaced by a variation with a minimal depth.

    Control-Unitaries (e.g. CZ and CNOT) commute when their first operands are the same qubit.
    Furthermore, CNOTs in addition commute when their second operands are the same qubit.
    The OpenQL depgraph construction recognizes these and represents these in the dependence graph:
    - The Control-Unitary's first operands are seen as Reads.
      On each such Read a dependence is created
      from the last Write (RAW) or last D (RAD) (i.e. last non-Read) to the Control-Unitary,
      and on each first Write or D (i.e. first non-Read) after a set of Reads,
      dependences are created from those Control-Unitaries to that first Write (WAR) or that first D (DAR).
    - The CNOT's second operands are seen as Ds (the D stands for controlleD).
      On each such D a dependence is created
      from the last Write (DAW) or last Read (DAR) (i.e. last non-D) to the CNOT,
      and on each first Write or Read (i.e. first non-D) after a set of Ds,
      dependences are created from those CNOTs to that first Write (WAD) or that first Read (RAD).
    The commutable sets of Control-Unitaries (resp. CNOTs) can be found in the dependence graph
    by finding those first non-Read (/first non-D) nodes that have such incoming WAR/DAR (/WAD/RAD) dependences
    and considering the nodes that those incoming dependences come from; those nodes form the commutable sets.
    Recognition of commutation is enabled during dependence graph construction by setting the option scheduler_commute to yes.

    The generation of all these variations is done as follows:
    - At each node in the dependence graph, check its incoming dependences
      whether this node is such a first non-Read or first non-D use;
      those incoming dependences are ordered by their dependence type and their cause (the qubit causing the dependence),
      - when WAR/DAR then we have commutation on a Read operand (1st operand of CNOT, both operands of CZ),
        the cause represents the operand qubit
      - when WAD/RAD then we have commutation on a D operand (2nd operand of CNOT),
        the cause represents the operand qubit
      and the possibly several sets of commutable gates are filtered out from these incoming dependences.
      Each commutable set is represented by a list of arcs in the depgraph, i.e. arcs representing dependences
      from the node representing one of the commutable gates and to the gate with the first non-Read/D use.
      Note that in one set, of all incoming dependences the deptypes (WAR, DAR, WAD or RAD) must agree
      and the causes must agree.
      Each such set of commutable gates gives rise to a set of variations: all permutations of the gates.
      The number of those is the factorial of the size of the commutable set.
    - All these sets of commutable gates are stored in a list of such, the varslist.
      All sets together lead to a maximum number of variations that is the multiplication of those factorials.
      All variations can be enumerated by varying lexicographically
      through those combinations of permutations (a kind of goedelisation).
      One permutation of one commutable set stands for a particular order of the gates in the set;
      in the depgraph this order can be enforced by adding to the depgraph RAR (for sets of Control-Unitaries)
      or DAD (for sets of CNOT 2nd operand commutable gates) dependences between the gates in the set, from first to last.
    - Then for each variation:
      - the dependences are added
      - tested whether the dependence graph is still acyclic; when the dependence graph became cyclic
        after having added the RAR/DAD dependences, some commutable sets were interfering, i.e. there were
        additional dependences (on the other operands) between members of those commutable sets that enforce an order
        between particular pairs of members of those sets;
        when the dependence graph became cyclic, this variation is not feasible and can be skipped
      - a schedule is computed and its depth and variation number are kept
      - the schedule is optionally printed with the variation number in its name
      - and in any case then the added dependences are deleted so that the depgraph is restored to its original state.
    One of the variations with the least depth is stored in the current circuit as result of this variation search.
    Also, the scheduler_commute option is turned off so that future schedulers will respect the found order.
*/

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
    typedef unsigned long   vc_t;

// Scheduler class extension with entries to find the variations based on the dependence graph.
class Depgraph : public Scheduler
{
private:
    // variation count multiply aiming to catch overflow
    vc_t mult(vc_t a, vc_t b)
    {
        vc_t    r = a * b;
        if (r < a || r < b)
        {
            EOUT("Error: number of variations more than fits in unsigned long");
            throw ql::exception("[x] Error : number of variations more than fits in unsigned long!", false);
        }
        return r;
    }

public:
    // after scheduling, delete the added arcs (RAR/DAD) from the depgraph to restore it to the original state
    void clean_variation( std::list<ListDigraph::Arc>& newarcslist)
    {
        for ( auto a : newarcslist)
        {
            // DOUT("...... erasing arc from " << instruction[graph.source(a)]->qasm() << " to " << instruction[graph.target(a)]->qasm() << " as " << DepTypesNames[depType[a]] << " by q" << cause[a]);
            graph.erase(a);
        }
        newarcslist.clear();
    }

    // return encoding string of variation var for debugging output
    std::string varstring(std::list<std::list<ListDigraph::Arc>>& varslist, vc_t var)
    {
        std::ostringstream ss;

        int varslist_index = 1;
        for ( auto subvarslist : varslist )
        {
            if (varslist_index != 1)
            {
                ss << "|";
            }
            for (auto svs = subvarslist.size(); svs != 0; svs--)
            {
                if (svs != subvarslist.size())
                {
                    ss << "-";
                }
                auto thisone = var % svs;
                ss << thisone;
                var = var / svs;
            }
            varslist_index++;
        }
        return ss.str();
    }

    // make this variation effective by generating a sequentialization for the nodes in each subvarslist
    // the sequentialization is done by adding RAR/DAD dependences to the dependence graph;
    // those are kept for removal again from the depgraph after scheduling
    // the original varslist is copied locally to a recipe_varslist that is gradually reduced to empty while generating
    void gen_variation(std::list<std::list<ListDigraph::Arc>>& varslist, std::list<ListDigraph::Arc>& newarcslist, vc_t var)
    {
        std::list<std::list<ListDigraph::Arc>> recipe_varslist = varslist;  // deepcopy, i.e. must copy each sublist of list as well
        DOUT("... variation " << var << " (" << varstring(varslist,var) << "):");
        // DOUT("... recipe_varslist.size()=" << recipe_varslist.size());
        int varslist_index = 1;
        for ( auto subvarslist : recipe_varslist )
        {
            // DOUT("... subvarslist index=" << varslist_index << " subvarslist.size()=" << subvarslist.size());
            bool prevvalid = false;             // add arc between each pair of nodes so skip 1st arc in subvarslist
            ListDigraph::Node    prevn = s;     // previous node when prevvalid==true; fake initialization by s
            for (auto svs = subvarslist.size(); svs != 0; svs--)
            {
                auto thisone = var % svs;     // gives 0 <= thisone < subvarslist.size()
                // DOUT("...... var=" << var << " svs=" << svs << " thisone=var%svs=" << thisone << " nextvar=var/svs=" << var/svs);
                auto li = subvarslist.begin();
                std::advance(li, thisone);
                ListDigraph::Arc a = *li;   // i.e. select the thisone's element in this subvarslist
                ListDigraph::Node n  = graph.source(a);
                // DOUT("...... set " << varslist_index << " take " << thisone << ": " << instruction[n]->qasm() << " as " << DepTypesNames[depType[a]] << " by q" << cause[a]);
                if (prevvalid)
                {
                    // DOUT("...... adding arc from " << instruction[prevn]->qasm() << " to " << instruction[n]->qasm());
                    auto newarc = graph.addArc(prevn, n);
                    weight[newarc] = weight[a];
                    cause[newarc] = cause[a];
                    depType[newarc] = (depType[a] == WAR ? RAR : DAD);
                    // DOUT("...... added arc from " << instruction[prevn]->qasm() << " to " << instruction[n]->qasm() << " as " << DepTypesNames[depType[newarc]] << " by q" << cause[newarc]);
                    newarcslist.push_back(newarc);
                }
                prevvalid = true;
                prevn = n;
                subvarslist.erase(li);      // take thisone's element out of the subvarslist, reducing it to list one element shorter
                var = var / svs;              // take out the current subvarslist.size() out of the encoding
            }
            varslist_index++;
        }
    }

    // split the incoming dependences (in arclist) into a separate set for each qubit cause
    // at the same time, compute the size of the resulting sets and from that the number of variations it results in
    // the running total (var_count) is multiplied by each of these resulting numbers to give the total number of variations
    // the individual sets are added as separate lists to varlist, which is a list of those individual sets
    // the individual sets are implemented as lists and are called subvarslist here
    void add_variations(std::list<ListDigraph::Arc>& arclist, std::list<std::list<ListDigraph::Arc>>& varslist, vc_t& var_count)
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
                vc_t perm_count = 1;
                std::list<ListDigraph::Arc> subvarslist;
                for ( auto a : TMParclist )
                {
                    ListDigraph::Node srcNode  = graph.source(a);
                    // DOUT("... " << instruction[srcNode]->qasm() << " as " << DepTypesNames[depType[a]] << " by q" << cause[a]);
                    perm_index++;
                    perm_count = mult(perm_count, perm_index);
                    subvarslist.push_back(a);
                }
                varslist.push_back(subvarslist);
                var_count = mult(var_count, perm_count);
            }
            arclist.remove_if([this,operand](ListDigraph::Arc a) { return cause[a] == operand; });
        }
    }

    // show the sets of commutable gates for debugging
    void show_sets(std::list<std::list<ListDigraph::Arc>>& varslist)
    {
        vc_t var_count = 1;
        auto list_index = 1;
        for ( auto subvarslist : varslist )
        {
            DOUT("Commuting set " << list_index << ":");
            int perm_index = 0;
            vc_t perm_count = 1;
            for (auto a : subvarslist)
            {
                ListDigraph::Node srcNode  = graph.source(a);
                DOUT("... " << instruction[srcNode]->qasm() << " as " << DepTypesNames[depType[a]] << " by q" << cause[a]);
                perm_index++;
                perm_count *= perm_index;
            }
            DOUT("Giving rise to " << perm_count << " variations");
            var_count *= perm_count;
            list_index++;
        }
        DOUT("Total " << var_count << " variations");
    }

    // for each node scan all incoming dependences
    // - when WAR/DAR then we have commutation on a Read operand (1st operand of CNOT, both operands of CZ);
    //   those incoming dependences are collected in Rarclist and further split by their cause in add_variations
    // - when WAD/RAD then we have commutation on a D operand (2nd operand of CNOT);
    //   those incoming dependences are collected in Darclist and further split by their cause in add_variations
    void find_variations(std::list<std::list<ListDigraph::Arc>>& varslist, vc_t& total)
    {
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

    // schedule the constructed depgraph for the platform with resource constraints and return the resulting depth
    size_t schedule_rc(const ql::quantum_platform& platform)
    {
        std::string schedopt = ql::options::get("scheduler");
        ql::ir::bundles_t   bundles;
        if ("ASAP" == schedopt)
        {
            resource_manager_t rm(platform, forward_scheduling);
            bundles = schedule_asap(rm, platform);
        }
        else if ("ALAP" == schedopt)
        {
            resource_manager_t rm(platform, backward_scheduling);
            bundles = schedule_alap(rm, platform);
        }
        else
        {
            EOUT("Unknown scheduler");
            throw ql::exception("Unknown scheduler!", false);
        }
        size_t depth = bundles.back().start_cycle + bundles.back().duration_in_cycles - bundles.front().start_cycle;
        return depth;
    }
};

// generate variations and keep the one with the least depth in the current kernel's circuit
class commute_variation
{
private:

    // print current circuit to a file in qasm format
    // use the variation number to create the file name
    // note that the scheduler has reordered the circuit's gates according to their assigned cycle value
    void print(quantum_kernel& kernel, vc_t varno)
    {
        std::stringstream ss_output_file;
        ss_output_file << ql::options::get("output_dir") << "/" << kernel.name << "_" << varno << ".qasm";
        DOUT("... writing variation to '" << ss_output_file.str() << "' ...");
        std::stringstream ss_qasm;
        ss_qasm << "." << kernel.name << "_" << varno << '\n';
        ql::circuit& ckt = kernel.c;
        for (auto gp : ckt)
        {
            ss_qasm << '\t' << gp->qasm();
            // ss_qasm << "\t# " << gp->cycle
            ss_qasm << '\n';
        }
        size_t  depth = ckt.back()->cycle + std::ceil( static_cast<float>(ckt.back()->duration) / kernel.cycle_time) - ckt.front()->cycle;
        ss_qasm <<  "# Depth=" << depth << '\n';
        ql::utils::write_file(ss_output_file.str(), ss_qasm.str());
    }

public:
    void generate(quantum_kernel& kernel, const ql::quantum_platform & platform, size_t nqubits, size_t ncreg = 0)
    {
        DOUT("Generate commutable variations of kernel circuit ...");
        ql::circuit& ckt = kernel.c;
        if (ckt.empty())
        {
            DOUT("Empty kernel " << kernel.name);
            return;
        }
        if (ql::options::get("scheduler_commute") == "no")
        {
            COUT("Scheduler_commute option is \"no\": don't generate commutation variations");
            DOUT("Scheduler_commute option is \"no\": don't generate commutation variations");
            return;
        }

        DOUT("Create a dependence graph and recognize commutation");
        Depgraph sched;
        sched.Init(ckt, platform, nqubits, ncreg);
    
        DOUT("Finding sets of commutable gates ...");
        std::list<std::list<ListDigraph::Arc>> varslist;
        vc_t total = 1;
        sched.find_variations(varslist, total);
        sched.show_sets(varslist);

        DOUT("Start enumerating " << total << " variations ...");
        DOUT("=========================\n\n");

        std::list<ListDigraph::Arc> newarcslist;        // new deps generated
        std::map<size_t,std::list<vc_t>> vars_per_depth;

        for (vc_t varno = 0; varno < total; varno++)
        {
            // generate additional (RAR or DAD) dependences to sequentialize this variation
            sched.gen_variation(varslist, newarcslist, varno);
            if ( !dag(sched.graph))
            {
                // there are cycles among the dependences so this variation is infeasible
                DOUT("... variation " << varno << " (" << sched.varstring(varslist,varno) << ") results in a dependence cycle, skip it");
            }
            else
            {
                // DOUT("... schedule variation " << varno << " (" << sched.varstring(varslist,varno) << ")");
                auto depth = sched.schedule_rc(platform);
                vars_per_depth[depth].push_back(varno);
                DOUT("... scheduled variation " << varno << " (" << sched.varstring(varslist,varno) << "), depth=" << depth);

                // DOUT("... printing qasm code for this variation " << varno << " (" << sched.varstring(varslist,varno) << ")");
                // print(kernel, varno);
            }
            // delete additional dependences generated so restore old depgraph with all commutation
            sched.clean_variation(newarcslist);
            // DOUT("... ready with variation " << varno << " (" << sched.varstring(varslist,varno) << ")");
            // DOUT("=========================\n");
        }
        DOUT("Generate commutable variations of kernel circuit [Done]");

        DOUT("Find circuit with minimum depth while exploiting commutation");
        for (auto vit = vars_per_depth.begin(); vit != vars_per_depth.end(); ++vit)
        {
            DOUT("... depth " << vit->first << ": " << vit->second.size() << " variations");
        }
        auto mit = vars_per_depth.begin();
        auto min_depth = mit->first;
        auto vars = mit->second;
        auto result_varno = vars.front();       // just the first one, could be more sophisticated
        DOUT("Min depth=" << min_depth << ", number of variations=" << vars.size() << ", selected varno=" << result_varno);

        // Find out which depth heuristics would find
        auto hdepth = sched.schedule_rc(platform);
        DOUT("Note that heuristics would find a schedule of the circuit with depth " << hdepth);

        // Set kernel.c representing result variation by regenerating it and scheduling it ...
        sched.gen_variation(varslist, newarcslist, result_varno);
        (void) sched.schedule_rc(platform); // sets kernel.c reflecting the additional deps of the variation
        sched.clean_variation(newarcslist);
        DOUT("Find circuit with minimum depth while exploiting commutation [Done]");

        ql::options::set("scheduler_commute", "no");    // next schedulers will respect the commutation order found
    }
};

}
}

#endif // QL_COMMUTE_VARIATION_H
