// general c++ library for manipulating streams
#include <iostream>
#include <sstream>
#include <stdexcept>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "Xilinx_MUXF8.hpp"

using namespace std;
namespace flopoco {
	Xilinx_MUXF8_base::Xilinx_MUXF8_base(Operator *parentOp, Target *target ) : Xilinx_Primitive( parentOp,target ) {}

	Xilinx_MUXF8::Xilinx_MUXF8(Operator *parentOp, Target *target ) : Xilinx_MUXF8_base( parentOp,target ) {
        setName( "MUXF8" );
        addOutput( "o" );
        base_init();
    }

	Xilinx_MUXF8_L::Xilinx_MUXF8_L(Operator *parentOp, Target *target ) : Xilinx_MUXF8_base( parentOp,target ) {
        setName( "MUXF8_L" );
        addOutput( "lo" );
        base_init();
    }

	Xilinx_MUXF8_D::Xilinx_MUXF8_D(Operator *parentOp, Target *target ) : Xilinx_MUXF8_base( parentOp,target ) {
        setName( "MUXF8_D" );
        addOutput( "lo" );
        addOutput( "o" );
        base_init();
    }

    void Xilinx_MUXF8_base::base_init() {
        // definition of the source file name, used for info and error reporting using REPORT
        srcFileName = "Xilinx_MUXF8";
        addInput( "i0" );
        addInput( "i1" );
        addInput( "s" );
    }
}//namespace
