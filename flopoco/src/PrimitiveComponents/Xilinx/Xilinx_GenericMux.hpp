#ifndef Xilinx_GenericMux_H
#define Xilinx_GenericMux_H

#include "Operator.hpp"
#include "utils.hpp"

namespace flopoco {

	// new operator class declaration
    class Xilinx_GenericMux : public Operator {
      private:
        int wordsize;
        int width;
        int sel_emu;
      public:
        Xilinx_GenericMux(Operator* parentOp, Target *target, int muxWidth, int wIn );

		// destructor
        ~Xilinx_GenericMux() {};
	};


}//namespace

#endif
