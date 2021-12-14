// general c++ library for manipulating streams
#include <iostream>
#include <sstream>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "Xilinx_GenericMux_slice.hpp"
#include "Xilinx_LUT6.hpp"
#include "Xilinx_MUXF7.hpp"
#include "Xilinx_MUXF8.hpp"
#include "Xilinx_CARRY4.hpp"
#include "Xilinx_Primitive.hpp"

using namespace std;

namespace flopoco {
    Xilinx_GenericMux_slice::Xilinx_GenericMux_slice(Operator *parentOp, Target *target, GenericMux_SLICE_VARIANT variant, int wIn ) : Operator( parentOp, target ) {
        setCopyrightString( "Marco Kleinlein" );

        setCombinatorial();
        stringstream namestr;
        namestr << "Xilinx_GenericMux";

        switch( variant ) {
        case GenericMux_SLICE_2:
            namestr << 2;
            break;

        case GenericMux_SLICE_4:
            namestr << 4;
            break;

        case GenericMux_SLICE_8:
            namestr << 8;
            break;

        case GenericMux_SLICE_16:
            namestr << 16;
            break;
        }

        namestr << "_slice_" << wIn;
        setNameWithFreqAndUID( namestr.str() );

        srcFileName = "Xilinx_GenericMux_slice";
        addOutput( "m_out", wIn );

        if( variant == GenericMux_SLICE_2 ) {
            lut_op op_o5 = ( ( ~lut_in( 4 ) & lut_in( 0 ) )
                             | ( lut_in( 4 ) & lut_in( 1 ) )
                           );
            lut_op op_o6 = ( ( ~lut_in( 4 ) & lut_in( 2 ) )
                             | ( lut_in( 4 ) & lut_in( 3 ) )
                           );
            lut_init lut_content( op_o5, op_o6 );
            lut5_init lut_dbg5( op_o5 );
            lut5_init lut_dbg6( op_o6 );
            REPORT( INFO, "LUT_o5: " + lut_dbg5.get_hex() );
            REPORT( DEBUG, "LUT_TABLE" + lut_dbg5.truth_table() );
            REPORT( INFO, "LUT_o6: " + lut_dbg6.get_hex() );
            REPORT( DEBUG, "LUT_TABLE" + lut_dbg6.truth_table() );
            REPORT( INFO, "LUT_CONTENT: " + lut_content.get_hex() );
            REPORT( DEBUG, "LUT_TABLE" + lut_content.truth_table() );
            addConstant( "current_lut_init", "bit_vector", lut_content.get_hex() );
            addInput( "m_in", wIn * 2 );
            addInput( "s_in" );

            for( int i = 0; i < ( wIn - ( wIn % 2 ) ) / 2; i++ ) {
				Xilinx_LUT6_2 *luts = new Xilinx_LUT6_2( parentOp, target );
                luts->setGeneric( "init", "current_lut_init", 64 );
                inPortMap( luts, join( "i", 0 ), "m_in" + of( 4 * i + 0 ) );
                inPortMap( luts, join( "i", 1 ), "m_in" + of( 4 * i + 1 ) );
                inPortMap( luts, join( "i", 2 ), "m_in" + of( 4 * i + 2 ) );
                inPortMap( luts, join( "i", 3 ), "m_in" + of( 4 * i + 3 ) );
                inPortMap( luts, join( "i", 4 ), "s_in" );
                inPortMapCst( luts, join( "i", 5 ), "'1'" );
                outPortMap( luts, "o5", "m_out" + of(2 * i));
                outPortMap( luts, "o6", "m_out" + of(2 * i + 1));
                stringstream lutname;
                lutname << "full_lut" << i;
                vhdl << luts->primitiveInstance( lutname.str() );
            }

            if( wIn % 2 ) {
				Xilinx_LUT6_2 *hluts = new Xilinx_LUT6_2( parentOp, target );
                hluts->setGeneric( "init", "current_lut_init", 64 );
                inPortMap( hluts, join( "i", 0 ), "m_in" + of( wIn * 2 - 2 ) );
                inPortMap( hluts, join( "i", 1 ), "m_in" + of( wIn * 2 - 1 ) );
                inPortMapCst( hluts, join( "i", 2 ), "'0'" );
                inPortMapCst( hluts, join( "i", 3 ), "'0'" );
                inPortMap( hluts, join( "i", 4 ), "s_in" );
                inPortMapCst( hluts, join( "i", 5 ), "'1'" );
                outPortMap( hluts, "o5", "m_out" + of( wIn - 1 ));
                vhdl << hluts->primitiveInstance( "half_lut" );
            }
        } else {
            lut_op op = ( ( ~lut_in( 5 ) & ~lut_in( 4 ) & lut_in( 0 ) )
                          | ( ~lut_in( 5 ) & lut_in( 4 ) & lut_in( 1 ) )
                          | ( lut_in( 5 ) & ~lut_in( 4 ) & lut_in( 2 ) )
                          | ( lut_in( 5 ) & lut_in( 4 ) & lut_in( 3 ) )
                        );
            lut_init lut_content( op );
            REPORT( INFO, "LUT_CONTENT: " + lut_content.get_hex() );
            REPORT( DEBUG, "LUT_TABLE" + lut_content.truth_table() );
            addConstant( "current_lut_init", "bit_vector", lut_content.get_hex() );

            if( variant == GenericMux_SLICE_4 ) {
                addInput( "m_in", wIn * 4 );
                addInput( "s_in", 2 );

                for( int i = 0; i < wIn; i++ ) {
					Xilinx_LUT6 *luts = new Xilinx_LUT6( parentOp, target );
                    luts->setGeneric( "init", "current_lut_init", 64 );
                    inPortMap( luts, join( "i", 0 ), "m_in" + of( 4 * i + 0 ) );
                    inPortMap( luts, join( "i", 1 ), "m_in" + of( 4 * i + 1 ) );
                    inPortMap( luts, join( "i", 2 ), "m_in" + of( 4 * i + 2 ) );
                    inPortMap( luts, join( "i", 3 ), "m_in" + of( 4 * i + 3 ) );
                    inPortMap( luts, join( "i", 4 ), "s_in" + of( 0 ) );
                    inPortMap( luts, join( "i", 5 ), "s_in" + of( 1 ) );
                    outPortMap( luts, "o",  "m_out" + of( i ));
                    stringstream lutname;
                    lutname << "full_lut" << i;
                    vhdl << luts->primitiveInstance( lutname.str() );
                }
            } else if( variant == GenericMux_SLICE_8 ) {
                addInput( "m_in", wIn * 8 );
                addInput( "s_in", 3 );
                declare( "intercon_L6_to_M7", wIn * 2 );

                for( int i = 0; i < 2 * wIn; i++ ) {
					Xilinx_LUT6_L *luts = new Xilinx_LUT6_L( parentOp,target );
                    luts->setGeneric( "init", "current_lut_init", 64 );
                    inPortMap( luts, join( "i", 0 ), "m_in" + of( 4 * i + 0 ) );
                    inPortMap( luts, join( "i", 1 ), "m_in" + of( 4 * i + 1 ) );
                    inPortMap( luts, join( "i", 2 ), "m_in" + of( 4 * i + 2 ) );
                    inPortMap( luts, join( "i", 3 ), "m_in" + of( 4 * i + 3 ) );
                    inPortMap( luts, join( "i", 4 ), "s_in" + of( 0 ) );
                    inPortMap( luts, join( "i", 5 ), "s_in" + of( 1 ) );
                    outPortMap( luts, "lo", "intercon_L6_to_M7" + of( i ));
                    stringstream lutname;
                    lutname << "full_lut" << i;
                    vhdl << luts->primitiveInstance( lutname.str() );
                }

                for( int i = 0; i < wIn; i++ ) {
					Xilinx_MUXF7 *mux7 = new Xilinx_MUXF7( parentOp,target );
                    inPortMap( mux7, join( "i", 0 ), "intercon_L6_to_M7" + of( i * 2 ) );
                    inPortMap( mux7, join( "i", 1 ), "intercon_L6_to_M7" + of( i * 2 + 1 ) );
                    inPortMap( mux7, "s", "s_in" + of( 2 ) );
                    outPortMap( mux7, "o", "m_out" + of( i ));
                    stringstream muxname;
                    muxname << "gen_mux7_" << i;
                    vhdl << mux7->primitiveInstance( muxname.str() );
                }
            } else if( variant == GenericMux_SLICE_16 ) {
                addInput( "m_in", wIn * 16 );
                addInput( "s_in", 4 );
                declare( "intercon_L6_to_M7", wIn * 4 );
                declare( "intercon_M7_to_M8", wIn * 2 );

                for( int i = 0; i < wIn * 4; i++ ) {
					Xilinx_LUT6_L *luts = new Xilinx_LUT6_L( parentOp,target );
                    luts->setGeneric( "init", "current_lut_init", 64 );
                    inPortMap( luts, join( "i", 0 ), "m_in" + of( 4 * i + 0 ) );
                    inPortMap( luts, join( "i", 1 ), "m_in" + of( 4 * i + 1 ) );
                    inPortMap( luts, join( "i", 2 ), "m_in" + of( 4 * i + 2 ) );
                    inPortMap( luts, join( "i", 3 ), "m_in" + of( 4 * i + 3 ) );
                    inPortMap( luts, join( "i", 4 ), "s_in" + of( 0 ) );
                    inPortMap( luts, join( "i", 5 ), "s_in" + of( 1 ) );
                    outPortMap( luts, "lo", "intercon_L6_to_M7" + of( i ));
                    stringstream lutname;
                    lutname << "full_lut" << i;
                    vhdl << luts->primitiveInstance( lutname.str() );
                }

                for( int i = 0; i < wIn * 2; i++ ) {
					Xilinx_MUXF7_L *mux7 = new Xilinx_MUXF7_L( parentOp,target );
                    inPortMap( mux7, join( "i", 0 ), "intercon_L6_to_M7" + of( i * 2 ) );
                    inPortMap( mux7, join( "i", 1 ), "intercon_L6_to_M7" + of( i * 2 + 1 ) );
                    inPortMap( mux7, "s", "s_in" + of( 2 ) );
                    outPortMap( mux7, "o", "intercon_M7_to_M8" + of( i ));
                    stringstream muxname;
                    muxname << "gen_mux7_" << i;
                    vhdl << mux7->primitiveInstance( muxname.str() );
                }

				Xilinx_MUXF8 *mux8 = new Xilinx_MUXF8( parentOp,target );
                inPortMap( mux8, join( "i", 0 ), "intercon_M7_to_M8" + of( 0 ) );
                inPortMap( mux8, join( "i", 1 ), "intercon_M7_to_M8" + of( 1 ) );
                inPortMap( mux8, "s", "s_in" + of( 3 ) );
                outPortMap( mux8, "o", "m_out" + of( 0 ));
                vhdl << mux8->primitiveInstance( "gen_mux8" );
            }
        }
    }
} //namespace
