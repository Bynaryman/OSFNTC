// general c++ library for manipulating streams
#include <iostream>
#include <sstream>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "Xilinx_GenericAddSub_slice.hpp"
#include "Xilinx_LUT6.hpp"
#include "Xilinx_CARRY4.hpp"

using namespace std;

namespace flopoco {
    Xilinx_GenericAddSub_slice::Xilinx_GenericAddSub_slice(Operator *parentOp, Target *target, int wIn, bool initial, bool fixed, bool dss , const string &prefix ) : Operator( parentOp, target ) {
        setCopyrightString( "Marco Kleinlein" );

        stringstream name;

        if( prefix.empty() ) {
            name << "Xilinx_GenericAddSub_slice_" << wIn;
        } else {
            name << prefix << "_slice" << wIn;
        }

        if( dss ) {
            name << "_dss";
        }

        if( initial ) {
            name << "_init";
        }

        setNameWithFreqAndUID( name.str() );
        setCombinatorial();
        srcFileName = "Xilinx_GenericAddSub_slice";
        REPORT( DEBUG , "Building" + this->getName() );

        if( dss ) {
			build_with_dss( parentOp, target, wIn, initial );
        } else if( fixed ) {
			build_fixed_sign( parentOp, target, wIn, initial );
        } else {
			build_normal( parentOp, target, wIn, initial );
        }
    }

	void Xilinx_GenericAddSub_slice::build_normal(Operator* parentOp, Target *target, int wIn, bool initial ) {
        lut_op carry_pre_o6;
        lut_op carry_pre_o5 = lut_in( 2 ) | lut_in( 3 );
        lut_op add_o5 =
            ( ~lut_in( 2 ) & ~lut_in( 3 ) &  lut_in( 0 ) ) |
            ( lut_in( 2 ) & ~lut_in( 3 ) & ~lut_in( 0 ) ) |
            ( ~lut_in( 2 ) &  lut_in( 3 ) &  lut_in( 0 ) ) ;
        lut_op add_o6 =
            ( ~lut_in( 2 ) & ~lut_in( 3 ) & ( lut_in( 0 )^ lut_in( 1 ) ) ) |
            ( lut_in( 2 ) & ~lut_in( 3 ) & ( ~lut_in( 0 )^ lut_in( 1 ) ) ) |
            ( ~lut_in( 2 ) &  lut_in( 3 ) & ( lut_in( 0 )^ ~lut_in( 1 ) ) ) ;
        lut_init carry_pre( carry_pre_o5, carry_pre_o6 );
        lut_init adder( add_o5, add_o6 );
        addInput( "x_in", wIn );
        addInput( "y_in", wIn );
        addInput( "neg_x_in" );
        addInput( "neg_y_in" );
        addInput( "carry_in" );
        addOutput( "carry_out" );
        addOutput( "sum_out", wIn );
        declare( "cc_di", 4 );
        declare( "cc_s", 4 );
        declare( "cc_o", 4 );
        declare( "cc_co", 4 );
        declare( "lut_o5", wIn );
        declare( "lut_o6", wIn );

        if ( wIn < 4 ) {
            stringstream fillup;

            fillup << "(3 downto " << ( wIn ) << " => '0')";

            vhdl << tab << "cc_di" << range( 3, wIn ) << " <= " << fillup.str() << ";" << std::endl;
            vhdl << tab << "cc_s" << range( 3, wIn ) << " <= " << fillup.str() << ";" << std::endl;
            vhdl << tab << "cc_di" << range( wIn - 1, 0 ) << " <= lut_o5;" << std::endl;
            vhdl << tab << "cc_s" << range( wIn - 1, 0 ) << " <= lut_o6;" << std::endl;
        } else {
            vhdl << tab << "cc_di" << " <= lut_o5;" << std::endl;
            vhdl << tab << "cc_s" << " <= lut_o6;" << std::endl;
        }

        for( int i = 0; i < wIn; i++ ) {
            stringstream lut_name;
            lut_name << "lut_bit_" << i;
			Xilinx_LUT6_2 *initial_lut = new Xilinx_LUT6_2( parentOp,target );

            if( initial && i == 0 ) {
                initial_lut->setGeneric( "init", carry_pre.get_hex(), 64 );
            } else {
                initial_lut->setGeneric( "init", adder.get_hex(), 64 );
            }

            inPortMap( initial_lut, "i0", "y_in" + of( i ) );
            inPortMap( initial_lut, "i1", "x_in" + of( i ) );
            inPortMap( initial_lut, "i2", "neg_y_in" );
            inPortMap( initial_lut, "i3", "neg_x_in" );

            if( initial && i == 0 ) {
                inPortMapCst( initial_lut, "i4", "'0'" );
            } else {
                inPortMapCst( initial_lut, "i4", "'1'" );
            }

            inPortMapCst( initial_lut, "i5", "'1'" );
            outPortMap( initial_lut, "o5", "lut_o5" + of( i ));
            outPortMap( initial_lut, "o6", "lut_o6" + of( i ));
            vhdl << initial_lut->primitiveInstance( lut_name.str() );
        }

		Xilinx_CARRY4 *further_cc = new Xilinx_CARRY4( parentOp,target );
        outPortMap( further_cc, "co", "cc_co");
        outPortMap( further_cc, "o", "cc_o");
        inPortMapCst( further_cc, "cyinit", "'0'" );
        inPortMap( further_cc, "ci", "carry_in" );
        inPortMap( further_cc, "di", "cc_di" );
        inPortMap( further_cc, "s", "cc_s" );
        vhdl << further_cc->primitiveInstance( "slice_cc" );
        vhdl << "carry_out <= cc_co" << of( wIn - 1 ) << ";" << std::endl;
        vhdl << "sum_out <= cc_o" << range( wIn - 1, 0 ) << ";" << std::endl;
    }

	void Xilinx_GenericAddSub_slice::build_fixed_sign(Operator* parentOp, Target *target, int wIn, bool initial ) {
        lut_op add_o5 =   ( ~lut_in( 2 ) & ~lut_in( 3 ) &  lut_in( 0 ) ) |
                          ( lut_in( 2 ) & ~lut_in( 3 ) & ~lut_in( 0 ) ) |
                          ( ~lut_in( 2 ) &  lut_in( 3 ) &  lut_in( 0 ) ) ;
        lut_op add_o6 =   ( ~lut_in( 2 ) & ~lut_in( 3 ) & ( lut_in( 0 )^ lut_in( 1 ) ) ) |
                          ( lut_in( 2 ) & ~lut_in( 3 ) & ( ~lut_in( 0 )^ lut_in( 1 ) ) ) |
                          ( ~lut_in( 2 ) &  lut_in( 3 ) & ( lut_in( 0 ) ^ ~lut_in( 1 ) ) ) ;
        lut_init adder( add_o5, add_o6 );
        addInput( "x_in", wIn );
        addInput( "y_in", wIn );
        addInput( "neg_x_in" );
        addInput( "neg_y_in" );
        addInput( "carry_in" );
        addOutput( "carry_out" );
        addOutput( "sum_out", wIn );
        declare( "cc_di", 4 );
        declare( "cc_s", 4 );
        declare( "cc_o", 4 );
        declare( "cc_co", 4 );
        declare( "lut_o5", wIn );
        declare( "lut_o6", wIn );

        if ( wIn < 4 ) {
            stringstream fillup;

            if ( 4 - wIn == 1 ) {
                fillup << "'0'";
            } else {
                fillup << "(3 downto " << ( wIn ) << " => '0')";
            }

            vhdl << tab << "cc_di" << range( 3, wIn ) << " <= " << fillup.str() << ";" << std::endl;
            vhdl << tab << "cc_s" << range( 3, wIn ) << " <= " << fillup.str() << ";" << std::endl;
            vhdl << tab << "cc_di" << range( wIn - 1, 0 ) << " <= lut_o5;" << std::endl;
            vhdl << tab << "cc_s" << range( wIn - 1, 0 ) << " <= lut_o6;" << std::endl;
        } else {
            vhdl << tab << "cc_di" << " <= lut_o5;" << std::endl;
            vhdl << tab << "cc_s" << " <= lut_o6;" << std::endl;
        }

        for( int i = 0; i < wIn; i++ ) {
            stringstream lut_name;
            lut_name << "lut_bit_" << i;
			Xilinx_LUT6_2 *initial_lut = new Xilinx_LUT6_2( parentOp,target );
            initial_lut->setGeneric( "init", adder.get_hex(), 64 );
            inPortMap( initial_lut, "i0", "y_in" + of( i ) );
            inPortMap( initial_lut, "i1", "x_in" + of( i ) );
            inPortMap( initial_lut, "i2", "neg_y_in" );
            inPortMap( initial_lut, "i3", "neg_x_in" );
            inPortMapCst( initial_lut, "i4", "'1'" );
            inPortMapCst( initial_lut, "i5", "'1'" );
            outPortMap( initial_lut, "o5", "lut_o5" + of( i ));
            outPortMap( initial_lut, "o6", "lut_o6" + of( i ));
            vhdl << initial_lut->primitiveInstance( lut_name.str() );
        }

		Xilinx_CARRY4 *further_cc = new Xilinx_CARRY4( parentOp,target );
        outPortMap( further_cc, "co", "cc_co");
        outPortMap( further_cc, "o", "cc_o");
        inPortMapCst( further_cc, "cyinit", "'0'" );
        inPortMap( further_cc, "ci", "carry_in" );
        inPortMap( further_cc, "di", "cc_di" );
        inPortMap( further_cc, "s", "cc_s" );
        vhdl << further_cc->primitiveInstance( "slice_cc" );
        vhdl << "carry_out <= cc_co" << of( wIn - 1 ) << ";" << std::endl;
        vhdl << "sum_out <= cc_o" << range( wIn - 1, 0 ) << ";" << std::endl;
    }

	void Xilinx_GenericAddSub_slice::build_with_dss(Operator *parentOp, Target *target, int wIn, bool initial ) {
        addInput( "x_in", wIn );
        addInput( "y_in", wIn );
        addInput( "neg_x_in" );
        addInput( "neg_y_in" );
        addInput( "carry_in" );
        addOutput( "carry_out" );
        addOutput( "sum_out", wIn );
        addInput( "bbus_in", wIn );
        addOutput( "bbus_out", wIn );
        declare( "cc_di", 4 );
        declare( "cc_s", 4 );
        declare( "cc_o", 4 );
        declare( "cc_co", 4 );
        declare( "lut_o5", wIn );
        declare( "lut_o6", wIn );
        declare( "bb_t", wIn );

        if ( wIn < 4 ) {
            stringstream fillup;

            if ( 4 - wIn == 1 ) {
                fillup << "'0'";
            } else {
                fillup << "(3 downto " << ( wIn ) << " => '0')";
            }

            vhdl << tab << "cc_di" << range( 3, wIn ) << " <= " << fillup.str() << ";" << std::endl;
            vhdl << tab << "cc_s" << range( 3, wIn ) << " <= " << fillup.str() << ";" << std::endl;
            vhdl << tab << "cc_di" << range( wIn - 1, 0 ) << " <= bbus_in;" << std::endl;
            vhdl << tab << "cc_s" << range( wIn - 1, 0 ) << " <= lut_o6;" << std::endl;
        } else {
            vhdl << tab << "cc_di" << " <= bbus_in;" << std::endl;
            vhdl << tab << "cc_s" << " <= lut_o6;" << std::endl;
        }

        for( int i = 0; i < wIn; i++ ) {
            stringstream lut_name;
            lut_name << "lut_bit_" << i;
			Xilinx_LUT6_2 *cur_lut = new Xilinx_LUT6_2( parentOp,target );

            if( initial && i == 0 ) {
                cur_lut->setGeneric( "init", getLUT_dss_init(), 64 );
            } else if( initial && i == 1 ) {
                cur_lut->setGeneric( "init", getLUT_dss_sec(), 64 );
            } else {
                cur_lut->setGeneric( "init", getLUT_dss_std(), 64 );
            }

            inPortMap( cur_lut, "i0", "y_in" + of( i ) );
            inPortMap( cur_lut, "i1", "x_in" + of( i ) );
            inPortMap( cur_lut, "i2", "neg_y_in" );
            inPortMap( cur_lut, "i3", "neg_x_in" );
            inPortMap( cur_lut, "i4", "bbus_in" + of( i ) );
            inPortMapCst( cur_lut, "i5", "'1'" );
            outPortMap( cur_lut, "o5", "bb_t" + of( i ));
            outPortMap( cur_lut, "o6", "lut_o6" + of( i ));
            vhdl << cur_lut->primitiveInstance( lut_name.str() );
        }

		Xilinx_CARRY4 *further_cc = new Xilinx_CARRY4( parentOp,target );
        outPortMap( further_cc, "co", "cc_co");
        outPortMap( further_cc, "o", "cc_o");
        inPortMapCst( further_cc, "cyinit", "'0'" );
        inPortMap( further_cc, "ci", "carry_in" );
        inPortMap( further_cc, "di", "cc_di" );
        inPortMap( further_cc, "s", "cc_s" );
        vhdl << further_cc->primitiveInstance( "slice_cc" );
        vhdl << "carry_out <= cc_co" << of( wIn - 1 ) << ";" << std::endl;
        vhdl << "sum_out <= cc_o" << range( wIn - 1, 0 ) << ";" << std::endl;
        vhdl << "bbus_out <= bb_t;" << std::endl;
    }

    string Xilinx_GenericAddSub_slice::getLUT_dss_init() {
        lut_op fa_s = ( lut_in( 0 )^lut_in( 2 ) ) ^ ( lut_in( 1 )^lut_in( 3 ) ) ^ ( lut_in( 2 )^lut_in( 3 ) );
        lut_op fa_c =
            ( ( lut_in( 0 )^lut_in( 2 ) ) & ( lut_in( 1 )^lut_in( 3 ) ) ) |
            ( ( lut_in( 0 )^lut_in( 2 ) ) & ( lut_in( 2 )^lut_in( 3 ) ) ) |
            ( ( lut_in( 1 )^lut_in( 3 ) ) & ( lut_in( 2 )^lut_in( 3 ) ) );
        lut_op o5 = fa_c;
        lut_op o6 = fa_s;
        lut_init op( o5, o6 );
        return op.get_hex();
    }

    string Xilinx_GenericAddSub_slice::getLUT_dss_sec() {
        lut_op fa_s = ( lut_in( 0 )^lut_in( 2 ) ) ^ ( lut_in( 1 )^lut_in( 3 ) ) ^ ( lut_in( 2 )&lut_in( 3 ) );
        lut_op fa_c =
            ( ( lut_in( 0 )^lut_in( 2 ) ) & ( lut_in( 1 )^lut_in( 3 ) ) ) |
            ( ( lut_in( 0 )^lut_in( 2 ) ) & ( lut_in( 2 )&lut_in( 3 ) ) ) |
            ( ( lut_in( 1 )^lut_in( 3 ) ) & ( lut_in( 2 )&lut_in( 3 ) ) );
        lut_op o5 = fa_c;
        lut_op o6 = fa_s ^ lut_in( 4 );
        lut_init op( o5, o6 );
        return op.get_hex();
    }

    string Xilinx_GenericAddSub_slice::getLUT_dss_std() {
        lut_op fa_s = ( lut_in( 0 )^lut_in( 2 ) ) ^ ( lut_in( 1 )^lut_in( 3 ) );
        lut_op fa_c = ( ( lut_in( 0 )^lut_in( 2 ) ) & ( lut_in( 1 )^lut_in( 3 ) ) );
        lut_op o5 = fa_c;
        lut_op o6 = fa_s ^ lut_in( 4 );
        lut_init op( o5, o6 );
        return op.get_hex();
    };
}//namespace
