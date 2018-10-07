/**
 * @file   optimizer.h
 * @date   11/2016
 * @author Nader Khammassi
 * @brief  optimizer interface and its implementation
 * @todo   implementations should be in separate files for better readability
 */
#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "utils.h"
#include "circuit.h"


namespace ql
{
   /**
    * optimizer interface
    */
   class optimizer
   {
      public:
         virtual circuit optimize(circuit& c) = 0;
   };

}

   #include "rot_merge.h"
   #include "kernel_splitter.h"



#endif // OPTIMIZER_H
