#ifndef Xilinx_CARRY4_H
#define Xilinx_CARRY4_H

#include "Xilinx_Primitive.hpp"

namespace flopoco {

    // new operator class declaration
    class Xilinx_CARRY4 : public Xilinx_Primitive {
      public:

        // constructor, defined there with two parameters (default value 0 for each)
		Xilinx_CARRY4(Operator *parentOp, Target *target );

        // destructor
        ~Xilinx_CARRY4() {};
    };
}//namespace

#endif
