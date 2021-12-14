#ifndef Xilinx_GenericMux_slice_H
#define Xilinx_GenericMux_slice_H

#include "Operator.hpp"
#include "utils.hpp"


namespace flopoco {
    enum GenericMux_SLICE_VARIANT {
        GenericMux_SLICE_2,
        GenericMux_SLICE_4,
        GenericMux_SLICE_8,
        GenericMux_SLICE_16
    };

	// new operator class declaration
    class Xilinx_GenericMux_slice : public Operator {
      public:
        Xilinx_GenericMux_slice(Operator *parentOp, Target *target, GenericMux_SLICE_VARIANT variant, int wIn );
        ~Xilinx_GenericMux_slice() {};
	};


}//namespace

#endif
