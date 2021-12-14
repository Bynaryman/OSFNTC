#ifndef Xilinx_TernaryAdd_2State_slice_H
#define Xilinx_TernaryAdd_2State_slice_H

#include "Operator.hpp"
#include "utils.hpp"

namespace flopoco {

    class Xilinx_TernaryAdd_2State_slice : public Operator {
      private:
        string build_lutinit_function();

      public:
        Xilinx_TernaryAdd_2State_slice( Operator *parentOp, Target *target, const uint &wIn = 4, const bool &is_initial = false, const std::string &lut_content = "x\"69699696e8e8e8e8\"" );

        ~Xilinx_TernaryAdd_2State_slice() {}
	};


}//namespace

#endif
