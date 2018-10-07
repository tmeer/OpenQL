/**
 * @file   kernel_splitter.h
 * @date   09/2018
 * @author Nader Khammassi
 * @brief  split quantum circuit into basic blocks and extract sequences
 *         for local optimizations
 */
#ifndef KERNEL_SPLITTER_H
#define KERNEL_SPLITTER_H

#include "utils.h"
#include "circuit.h"

namespace ql
{
   // type defs
   typedef std::vector<size_t> qubits_t;

   class node_t;

   /**
    * node
    */
   class node_t
   {

      public: 

         std::vector<node_t *> inputs;
         std::vector<node_t *> outputs;
         ql::gate *            gate;
         bool                  executed;

         /**
          * ctor
          */
         node_t(ql::gate * gate) : gate(gate), executed(false)
         { }

         /**
          * dot
          */
         void dot()
         {
            for (auto n : outputs)
               println("g" << gate << " -> g" << n->gate);
         }
   };

   /**
    * dependency analysis
    */
   class gate_dependency_graph
   {
      public:
         size_t                 num_qubits;
         ql::circuit *          c;  
         std::vector<node_t *>  r_nodes;
         std::vector<node_t *>  i_nodes;

         /**
          * ctor
          */
         gate_dependency_graph(size_t num_qubits, ql::circuit * c) : num_qubits(num_qubits), c(c)
         {
         }

         /**
          * build graph
          */
         void build()
         {
            std::vector<node_t *> record(num_qubits, NULL); 

            for (ql::gate * g : *c)
            {
               node_t * n = new node_t(g);
               i_nodes.push_back(n);
               ql::qubits_t qubits = g->operands;
               // println("gate : " << gate_type[g->type()]);
               // println("  |- qubits : " << qubits.size());
               for (auto q : qubits)
               {
                  node_t * l = record[q];
                  if (l != NULL) 
                  {
                     n->inputs.push_back(l);
                     l->outputs.push_back(n);
                     // println(" |- " << gate_type[l->gate->type()] << " -> " << gate_type[n->gate->type()]);
                  }
                  record[q] = n;
                  if (n->inputs.empty())
                     r_nodes.push_back(n);
               }
            }
         }

         /**
          * 
          */
         void deps_analysis(node_t * n,  std::vector<ql::gate *>& deps, bool clear=true) 
         {
            if (clear) deps.clear(); // clear when root node
            if (!n->executed) { deps.push_back(n->gate); n->executed = true; }
            if (!n->inputs.empty()) 
            {
               for (auto in : n->inputs)
                  if (!in->executed) { deps.push_back(in->gate); in->executed = true; }
               for (auto in : n->inputs)
                  deps_analysis(in,deps,false);
            }
            return;
         }

         /**
          * debug primitives
          */
         void print_outputs(node_t * n)
         {
            n->dot();
            if (n->outputs.empty())
               return;
            for (node_t * o : n->outputs)
               print_outputs(o);
         }

         /**
          * print graph
          */
         void print_graph()
         {
            println("digraph G {");
            // create nodes
            for (ql::gate * g : *c)
            {
               ql::qubits_t qubits = g->operands;
               std::cout << "g" << g->name << " [label=\"" << g->name << "s";
               if (qubits.empty()) 
               {
                  println("\"]");
                  continue;
               }
               std::cout << " q" << qubits[0];
               if (qubits.size() > 1)
                  for (size_t q=1; q<qubits.size(); q++)
                     std::cout << ",q" << qubits[q];
               println("\"]");
            }
            // for (node_t * n : r_nodes)
            // print_outputs(n);
            for (node_t * n : i_nodes)
               n->dot();
            println("}");
         }
   };


   /**
    * kernel splitter 
    */
   class kernel_splitter : public optimizer
   {
      public:

         size_t num_qubits;
         
         /**
          * kernel_splitter
          */
         kernel_splitter(size_t num_qubits, circuit& c) : num_qubits(num_qubits)
         {
         }

         /**
          * split
          */
         void split(gate_dependency_graph& g, circuit& rc)
         {
            std::vector<node_t *>  roots = g.r_nodes;
            std::vector<node_t *>  nodes = g.i_nodes;
            // extract local sequences
            circuit * current;
            current = new circuit;
            for (auto r : roots)
            {
               rc.push_back(r->gate);
               sequence(r,rc);
            }
         }

         /**
          * sequence
          */
         void sequence(node_t * cn, circuit& rc)
         {
            for (auto n : cn->outputs)
            {
               rc.push_back(n->gate);
               sequence(n,rc);
            }
         }

         /**
          * main transformation
          */
         circuit optimize(circuit& c)
         {
            circuit rc;
            gate_dependency_graph g(num_qubits,&c);
            g.build();
            split(g,rc);
            println("[+] rescheduled circuit : ");
            for (auto g : rc)
               println(g->qasm());
            return rc;
         }

   };
}

#endif // KERNEL_ANALYZER_H
