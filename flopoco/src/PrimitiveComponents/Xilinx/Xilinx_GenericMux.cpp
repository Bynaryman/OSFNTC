// general c++ library for manipulating streams
#include <iostream>
#include <sstream>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "Xilinx_GenericMux.hpp"
#include "Xilinx_GenericMux_slice.hpp"
#include "Xilinx_Primitive.hpp"

using namespace std;
namespace flopoco {
    Xilinx_GenericMux::Xilinx_GenericMux(Operator *parentOp, Target *target, int muxWidth, int wIn )
        : Operator( parentOp, target ),
          wordsize( wIn ),
          width( muxWidth ) {
        setCopyrightString( "Marco Kleinlein" );

        srcFileName = "Xilinx_GenericMux";
        stringstream namestr;
        namestr << "Xilinx_GenericMux_" << muxWidth << "_w" << wIn;
        setNameWithFreqAndUID( namestr.str() );
        setCombinatorial();
        int build_width = 2;
        int bit_per_slice = 0;
        GenericMux_SLICE_VARIANT slice_variant;

        if( muxWidth <= 2 ) {
            addInput( "s_in" );
            bit_per_slice = 8;
            build_width = 2;
            slice_variant = GenericMux_SLICE_2;
            addLUT( wIn );
        } else if( muxWidth <= 4 ) {
            addInput( "s_in", 2 );
            bit_per_slice = 4;
            build_width = 4;
            slice_variant = GenericMux_SLICE_4;
            addLUT( wIn );
        } else if( muxWidth <= 8 ) {
            addInput( "s_in", 3 );
            bit_per_slice = 2;
            build_width = 8;
            slice_variant = GenericMux_SLICE_8;
            addLUT( wIn * 2 );
        } else if( muxWidth <= 16 ) {
            addInput( "s_in", 4 );
            bit_per_slice = 1;
            build_width = 16;
            slice_variant = GenericMux_SLICE_16;
            addLUT( wIn * 4 );
        }

        stringstream conn_size_str;
        conn_size_str << build_width << " * input_word_size";
        int connector_size = build_width * wIn;
        int num_full_slices = ( wIn - ( wIn % bit_per_slice ) ) / bit_per_slice;
        int bits_remaining = ( wIn % bit_per_slice );
        declare( "mux_connector", connector_size );

        for( int i = 0; i < muxWidth; i++ )
            addInput( join( "x", i, "_in" ) , wIn );

        addOutput( "x_out", wIn );

        for( int i = 0; i < wIn; i++ ) {
            for( int j = 0; j < muxWidth - 1; j++ )
                vhdl << tab << "mux_connector" << of( i * build_width + j ) << " <= " << join( "x", j, "_in" ) << of( i ) << ";" << std::endl;

            if( muxWidth == build_width )
                vhdl << tab << "mux_connector" << of( i * build_width + muxWidth - 1 ) << " <= " << join( "x", muxWidth - 1, "_in" ) << of( i ) << ";" << std::endl;
            else {
                for( int f = muxWidth - 1; f < build_width; f++ )
                    vhdl << tab << "mux_connector" << of( i * build_width + f ) << " <= " << join( "x", muxWidth - 1, "_in" ) << of( i ) << ";" << std::endl;
            }
        }

        for( int i = 0; i < num_full_slices; i++ ) {
            Xilinx_GenericMux_slice *slice = new Xilinx_GenericMux_slice( parentOp, target, slice_variant, bit_per_slice );
            addSubComponent(slice);
            inPortMap( slice, "m_in", "mux_connector" + range( ( i + 1 ) * 16 - 1, i * 16 ) );
            inPortMap( slice, "s_in", "s_in" );
            outPortMap( slice, "m_out", "x_out" + range( ( i + 1 )*bit_per_slice - 1, i * bit_per_slice ));
            stringstream slice_name;
            slice_name << "full_slice" << i;
            vhdl << instance( slice, slice_name.str() );
        }

        if( bits_remaining > 0 ) {
            Xilinx_GenericMux_slice *part_slice = new Xilinx_GenericMux_slice( parentOp, target, slice_variant, bits_remaining );
            addSubComponent(part_slice);
            inPortMap( part_slice, "m_in", "mux_connector" + range( ( num_full_slices ) * 16 + build_width * bits_remaining - 1, num_full_slices * 16 ) );
            inPortMap( part_slice, "s_in", "s_in" );
            outPortMap( part_slice, "m_out", "x_out" + range( ( num_full_slices * bit_per_slice ) + bits_remaining - 1, ( num_full_slices * bit_per_slice ) ));
            vhdl << instance( part_slice, "part_slice" );
        }
    };
}//namespace
