// general c++ library for manipulating streams
#include <iostream>
#include <sstream>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "Xilinx_TernaryAdd_2State_slice.hpp"
#include "Xilinx_CARRY4.hpp"
#include "Xilinx_LUT6.hpp"

using namespace std;
namespace flopoco {

    Xilinx_TernaryAdd_2State_slice::Xilinx_TernaryAdd_2State_slice(Operator *parentOp, Target *target, const uint &wIn , const bool &is_initial , const std::string &lut_content ) : Operator( parentOp, target ) {
        setCopyrightString( "Marco Kleinlein" );


        setNameWithFreqAndUID( "Xilinx_TernaryAdd_2State_slice_s" + std::to_string( wIn ) + ( is_initial ? "_init" : "" ) );
        srcFileName = "Xilinx_TernaryAdd_2State_slice";
        //addToGlobalOpList( this );
        setCombinatorial();
        setShared();
        addInput( "sel_in" );
        addInput( "x_in", wIn );
        addInput( "y_in", wIn );
        addInput( "z_in", wIn );
        addInput( "bbus_in", wIn );
        addInput( "carry_in" );
        addOutput( "carry_out" );
        addOutput( "bbus_out", wIn );
        addOutput( "sum_out", wIn );
        declare( "lut_o6", wIn );
        declare( "cc_di", 4 );
        declare( "cc_s", 4 );
        declare( "cc_o", 4 );
        declare( "cc_co", 4 );
        addConstant( "fillup_width", "integer", join( "4 - ", wIn ) );

        if( wIn <= 0 || wIn > 4 ) {
            throw std::runtime_error( "A slice with " + std::to_string( wIn ) + " bits is not possible." );
        }

        if( wIn == 4 ) {
            vhdl << tab << tab << "cc_di <= bbus_in;" << endl;
            vhdl << tab << tab << "cc_s	 <= lut_o6;" << endl;
        } else {
            vhdl << tab << tab << "cc_di	<= (fillup_width downto 1 => '0') & bbus_in;" << endl;
            vhdl << tab << tab << "cc_s     <= (fillup_width downto 1 => '0') & lut_o6;" << endl;
        }

        for( uint i = 0; i < wIn; ++i  ) {
			Xilinx_LUT6_2 *lut_bit_i = new Xilinx_LUT6_2( this,target );
            lut_bit_i->setGeneric( "init", lut_content, 64 );
            inPortMap( lut_bit_i, "i0", "z_in" + of( i ) );
            inPortMap( lut_bit_i, "i1", "y_in" + of( i ) );
            inPortMap( lut_bit_i, "i2", "x_in" + of( i ) );
            inPortMap( lut_bit_i, "i3", "sel_in" );
            inPortMap( lut_bit_i, "i4", "bbus_in" + of( i ) );
            inPortMapCst( lut_bit_i, "i5", "'1'" );
            outPortMap( lut_bit_i, "o5", "bbus_out" + of( i ));
            outPortMap( lut_bit_i, "o6", "lut_o6" + of( i ));
            vhdl << lut_bit_i->primitiveInstance( join( "lut_bit_", i ) );
        }

        if( is_initial ) {
			Xilinx_CARRY4 *init_cc = new Xilinx_CARRY4( this,target );
            outPortMap( init_cc, "co", "cc_co");
            outPortMap( init_cc, "o", "cc_o");
            inPortMap( init_cc, "cyinit", "carry_in" );
            inPortMapCst( init_cc, "ci", "'0'" );
            inPortMap( init_cc, "di", "cc_di" );
            inPortMap( init_cc, "s", "cc_s" );
            vhdl << init_cc->primitiveInstance( "init_cc" );
        } else {
			Xilinx_CARRY4 *further_cc = new Xilinx_CARRY4( this,target );
            outPortMap( further_cc, "co", "cc_co");
            outPortMap( further_cc, "o", "cc_o");
            inPortMapCst( further_cc, "cyinit", "'0'" );
            inPortMap( further_cc, "ci", "carry_in" );
            inPortMap( further_cc, "di", "cc_di" );
            inPortMap( further_cc, "s", "cc_s" );
            vhdl << further_cc->primitiveInstance( "further_cc" );
        }

        vhdl << tab << "carry_out	<= cc_co" << of( wIn - 1 ) << ";" << std::endl;
        vhdl << tab << "sum_out <= cc_o" << range( wIn-1, 0 ) << ";" << std::endl;

	};

}//namespace
