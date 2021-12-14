// general c++ library for manipulating streams
#include <iostream>
#include <sstream>

// include the header of the Operator
#include "Xilinx_n2m_Decoder.hpp"
#include "Xilinx_LUT5.hpp"
#include "Xilinx_LUT6.hpp"
#include "Xilinx_LUT_compute.h"

using namespace std;

namespace flopoco {
    Xilinx_n2m_Decoder::Xilinx_n2m_Decoder(Operator *parentOp, Target *target, string name, map<int, int> groups, int n, int m ) : Operator( parentOp, target ) {
        setCopyrightString( "Marco Kleinlein" );

        setCombinatorial();
        stringstream tname;
        tname << "Xilinx_n2m_Decoder_" << name;
        srcFileName = "Xilinx_n2m_Decoder";
        setNameWithFreqAndUID( tname.str() );
        check_groups( groups, n, m );
        addInput( "x_in", n );
        addOutput( "x_out", m );
        declare( "x", ( n < 6 ? 5 : 6 ) );

        if( n > 6 )
            throw "not supported";

        stringstream fillstr;

        for( int i = 0; i < ( ( n < 6 ? 5 : 6 ) - ( int )n ); i++ ) fillstr << "0";

        if( fillstr.str().size() == 0 )
            vhdl << tab << "x <= x_in;" << endl << endl;
        else if( fillstr.str().size() == 1 )
            vhdl << tab << "x <= '0' & x_in;" << endl << endl;
        else
            vhdl << tab << "x <= \"" << fillstr.str() << "\" & x_in;" << endl << endl;

        for( int i = 0; i < ( int )m; i++ ) { //LUT Schleife
            lut_op lutc;

            for( map<int, int>::iterator iter = groups.begin(); iter != groups.end(); ++iter ) {
                if( ( *iter ).second & ( 1 << i ) ) {
                    int t = ( *iter ).first;
                    lut_op kanon;

                    for( int f = 0; f < 6; f++ ) {
                        if( ( t & ( 1 << f ) ) )
                            kanon = kanon & ( lut_in( f ) );
                        else
                            kanon = kanon & ( ~lut_in( f ) );
                    }

                    lutc = lutc | kanon;
                }
            }

            //lutc.print();

            if( n < 6 ) {
                lut5_init init( lutc );

                if( init.get() == 0 )
                    vhdl << tab << "x_out(" << i << ") <= '0';" << endl;
                else {
                    addLUT( 5, 1 );
					Xilinx_LUT5 *luti = new Xilinx_LUT5( parentOp, target );
                    inPortMap("i0", "x(0)" );
										inPortMap("i1", "x(1)" );
										inPortMap("i2", "x(2)" );
										inPortMap("i3", "x(3)" );
										inPortMap("i4", "x(4)" );
                    stringstream output;
                    output << "x_out(" << i << ")";
                    outPortMap("o", output.str());
                    luti->setGeneric( "init", init.get_hex(), 64 );
                    stringstream lutname;
                    lutname << "bit_" << i;
                    vhdl << luti->primitiveInstance( lutname.str() );
                }
            } else if( n == 6 ) {
                lut_init init( lutc );

                if( init.get() == 0 )
                    vhdl << tab << "x_out(" << i << ") <= '0';" << endl;
                else {
                    addLUT( 6, 1 );
					Xilinx_LUT6 *luti = new Xilinx_LUT6( parentOp, target );
                    inPortMap("i0", "x(0)" );
										inPortMap("i1", "x(1)" );
										inPortMap("i2", "x(2)" );
										inPortMap("i3", "x(3)" );
										inPortMap("i4", "x(4)" );
										inPortMap("i5", "x(5)" );
                    stringstream output;
                    output << "x_out(" << i << ")";
                    outPortMap( "o", output.str());
                    luti->setGeneric( "init", init.get_hex(), 64 );
                    stringstream lutname;
                    lutname << "bit_" << i;
                    vhdl << luti->primitiveInstance( lutname.str() );
                }
            }
        }
	};

    void Xilinx_n2m_Decoder::check_groups( map<int, int> &groups, int &n, int &m ) {
        int in_max_val = 0, out_max_val = 0;

        for( map<int, int>::iterator iter = groups.begin(); iter != groups.end(); ++iter ) {
            if( iter->first > in_max_val )
                in_max_val = iter->first;

            if( iter->second > out_max_val )
                out_max_val = iter->second;
        }

        int in_ws_need = 0;

        while( in_max_val > 0 ) {
            in_max_val = ( in_max_val >> 1 );
            in_ws_need++;
        }

        int out_ws_need = 0;

        while( out_max_val > 0 ) {
            out_max_val = ( out_max_val >> 1 );
            out_ws_need++;
        }

        if( n == 0 )
            n = in_ws_need;
        else if( n < in_ws_need )
            throw "Input wordsize does not fit input values";

        if( m == 0 )
            m = out_ws_need;
        else if( m < out_ws_need )
            throw "Output wordsize does not fit output values";
    }

}//namespace
