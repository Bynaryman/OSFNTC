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
#include "Xilinx_LUT5.hpp"

using namespace std;
namespace flopoco {
	Xilinx_LUT5_base::Xilinx_LUT5_base(Operator *parentOp, Target *target ) : Xilinx_Primitive( parentOp,target ) {}

	Xilinx_LUT5::Xilinx_LUT5(Operator *parentOp, Target *target ) : Xilinx_LUT5_base( parentOp,target ) {
        setName( "LUT5" );
        addOutput( "o" );
        base_init();
    }

	Xilinx_LUT5_L::Xilinx_LUT5_L(Operator *parentOp, Target *target ) : Xilinx_LUT5_base( parentOp,target ) {
        setName( "LUT5_L" );
        addOutput( "lo" );
        base_init();
    }

	Xilinx_LUT5_D::Xilinx_LUT5_D(Operator *parentOp, Target *target ) : Xilinx_LUT5_base( parentOp,target ) {
        setName( "LUT5_D" );
        addOutput( "lo" );
        addOutput( "o" );
        base_init();
    }

	Xilinx_CFGLUT5::Xilinx_CFGLUT5(Operator *parentOp, Target *target ) : Xilinx_LUT5_base( parentOp,target ) {
        setName( "CFGLUT5" );
        addInput("clk");
        addInput("ce"); //clk enable
        addInput("cdi");//configuration data in
        addOutput( "o5" ); //4 LUT output
        addOutput( "o6" ); //5 LUT output
        addOutput( "cdo" ); //configuration data out
        base_init();
    }

    void Xilinx_LUT5_base::base_init() {
        // definition of the source file name, used for info and error reporting using REPORT
        srcFileName = "Xilinx_LUT5";

        for( int i = 0; i < 5; i++ )
            addInput( join( "i", i ) );
    }
}//namespace
