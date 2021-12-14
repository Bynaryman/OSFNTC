// general c++ library for manipulating streams
#include <iostream>
#include <sstream>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "Xilinx_TernaryAdd_2State.hpp"
#include "Xilinx_TernaryAdd_2State_slice.hpp"
#include "PrimitiveComponents/Xilinx/Xilinx_Primitive.hpp"
#include "Xilinx_LUT_compute.h"

using namespace std;
namespace flopoco {

    Xilinx_TernaryAdd_2State::Xilinx_TernaryAdd_2State(Operator *parentOp, Target *target, const int &wIn, const short &bitmask, const short &bitmask2 )
        : Operator( parentOp, target ), wIn_(wIn), bitmask_(bitmask),bitmask2_(bitmask2) {
        setCopyrightString( "Marco Kleinlein" );
        if( bitmask2_ == -1 ) {
            bitmask2_ = bitmask_;
        }

        srcFileName = "Xilinx_TernaryAdd_2State";
        stringstream namestr;
        namestr << "Xilinx_TernaryAdd_2State_ws" << wIn << "_s" << ( bitmask_ & 0x7 );

        if( bitmask2_ != bitmask_ ) {
            namestr << "_s" << ( bitmask2_ & 0x7 );
        }

        setNameWithFreqAndUID( namestr.str() );
        string lut_content = computeLUT( );
        setCombinatorial();
        addInput( "x_i", wIn );
        addInput( "y_i", wIn );
        addInput( "z_i", wIn );

        if( bitmask_ == bitmask2_ ) {
            vhdl << declare( "sel_i" ) << " <= '0';" << std::endl;
        } else {
            addInput( "sel_i" );
        }

        addOutput( "sum_o", wIn );
        const uint num_slices = ( ( wIn - 1 ) / 4 ) + 1;
        declare( "x", wIn );
        declare( "y", wIn );
        declare( "z", wIn );
        declare( "bbus", wIn+1 );
        declare( "carry_cct" );
        declare( "carry", num_slices );

        if( wIn <= 0 ) {
            throw std::runtime_error( "An adder with wordsize 0 is not possible." );
        }

        insertCarryInit( );
        vhdl << tab << "x <= x_i;" << std::endl;
        vhdl << tab << "y <= y_i;" << std::endl;
        vhdl << tab << "z <= z_i;" << std::endl;

        if( num_slices == 1 ) {
            Xilinx_TernaryAdd_2State_slice *single_slice = new Xilinx_TernaryAdd_2State_slice( this, target, wIn, true, lut_content );
            addSubComponent( single_slice );
            inPortMap( single_slice, "x_in", "x" );
            inPortMap( single_slice, "y_in", "y" );
            inPortMap( single_slice, "z_in", "z" );
            inPortMap( single_slice, "sel_in", "sel_i" );
            inPortMap( single_slice, "bbus_in", "bbus" + range( wIn-1, 0 ) );
            inPortMap( single_slice, "carry_in", "carry_cct" );
            outPortMap( single_slice, "bbus_out", "bbus" + range( wIn, 1 ));
            outPortMap( single_slice, "carry_out", "carry" + of( 0 ));
            outPortMap( single_slice, "sum_out", "sum_o" + range( wIn-1, 0 ));
            vhdl << instance( single_slice, "slice_i" );
        }

        if ( num_slices > 1 ) {
            for( uint i = 0; i < num_slices; ++i ) {
                if( i == 0 ) {  // FIRST SLICE
                    Xilinx_TernaryAdd_2State_slice *first_slice = new Xilinx_TernaryAdd_2State_slice( this, target, 4, true, lut_content );
                    addSubComponent( first_slice );
                    inPortMap( first_slice, "x_in", "x" + range( 3, 0 ) );
                    inPortMap( first_slice, "y_in", "y" + range( 3, 0 ) );
                    inPortMap( first_slice, "z_in", "z" + range( 3, 0 ) );
                    inPortMap( first_slice, "sel_in", "sel_i" );
                    inPortMap( first_slice, "bbus_in", "bbus" + range( 3, 0 ) );
                    inPortMap( first_slice, "carry_in", "carry_cct" );
                    outPortMap( first_slice, "bbus_out", "bbus"  + range( 4, 1 ));
                    outPortMap( first_slice, "carry_out", "carry" + of( 0 ));
                    outPortMap( first_slice, "sum_out", "sum_o" + range( 3, 0 ));
                    vhdl << instance( first_slice, join( "slice_", i ) ) << endl;
                } else if( i == (num_slices - 1) ) { // LAST SLICE
                    Xilinx_TernaryAdd_2State_slice *last_slice = new Xilinx_TernaryAdd_2State_slice( this, target, wIn - ( 4 * i ), false, lut_content );
                    addSubComponent( last_slice );
                    inPortMap( last_slice, "x_in", "x" + range( wIn - 1, 4 * i ) );
                    inPortMap( last_slice, "y_in", "y" + range( wIn - 1, 4 * i ) );
                    inPortMap( last_slice, "z_in", "z" + range( wIn - 1, 4 * i ) );
                    inPortMap( last_slice, "sel_in", "sel_i" );
                    inPortMap( last_slice, "bbus_in", "bbus" + range( wIn - 1, 4 * i ) );
                    inPortMap( last_slice, "carry_in", "carry" + of( i - 1 ) );
                    outPortMap( last_slice, "bbus_out", "bbus" + range( wIn, 4 * i + 1 ));
                    outPortMap( last_slice, "carry_out", "carry" + of( i ));
                    outPortMap( last_slice, "sum_out", "sum_o" + range( wIn - 1, 4 * i ));
                    vhdl << instance( last_slice, join( "slice_", i ) ) << endl;
                } else {
                    Xilinx_TernaryAdd_2State_slice *full_slice = new Xilinx_TernaryAdd_2State_slice( this, target, 4, false, lut_content );
                    addSubComponent( full_slice );
                    inPortMap( full_slice, "x_in", "x" + range( ( 4 * i ) + 3, 4 * i ) );
                    inPortMap( full_slice, "y_in", "y" + range( ( 4 * i ) + 3, 4 * i ) );
                    inPortMap( full_slice, "z_in", "z" + range( ( 4 * i ) + 3, 4 * i ) );
                    inPortMap( full_slice, "sel_in", "sel_i" );
                    inPortMap( full_slice, "bbus_in", "bbus" + range( ( 4 * i ) + 3, 4 * i ) );
                    inPortMap( full_slice, "carry_in", "carry" + of( i - 1 ) );
                    outPortMap( full_slice, "bbus_out", "bbus" + range( ( 4 * i ) + 4, 4 * i + 1 ));
                    outPortMap( full_slice, "carry_out", "carry" + of( i ));
                    outPortMap( full_slice, "sum_out", "sum_o" + range( ( 4 * i ) + 3, 4 * i ));
                    vhdl << instance( full_slice, join( "slice_", i ) ) << endl;
                }
            }
        }
    };

	Xilinx_TernaryAdd_2State::~Xilinx_TernaryAdd_2State(){}

    string Xilinx_TernaryAdd_2State::computeLUT() {
        lut_op add_o5_co;
        lut_op add_o6_so;

        for( int i = 0; i < 32; ++i ) {
            int s = 0;

            for( int j = 0; j < 3; j++ ) {
                if( i & ( 0x8 ) ) {
                    if( ( ( bitmask2_ & ( 1 << ( 2 - j ) ) ) && !( i & ( 1 << j ) ) ) || ( !( bitmask2_ & ( 1 << ( 2 - j ) ) ) && ( i & ( 1 << j ) ) ) ) {
                        s++;
                    }
                } else {
                    if( ( ( bitmask_ & ( 1 << ( 2 - j ) ) ) && !( i & ( 1 << j ) ) ) || ( !( bitmask_ & ( 1 << ( 2 - j ) ) ) && ( i & ( 1 << j ) ) ) ) {
                        s++;
                    }
                }
            }

            if( s & 0x2 ) {
                lut_op co_part;

                for( int j = 0; j < 5; j++ ) {
                    if ( i & ( 1 << j ) ) {
                        co_part = co_part & lut_in( j );
                    } else {
                        co_part = co_part & ( ~lut_in( j ) );
                    }
                }

                add_o5_co = add_o5_co | co_part;
            }

            if( i & 0x10 ) {
                s++;
            }

            if( s & 0x1 ) {
                lut_op so_part;

                for( int j = 0; j < 5; j++ ) {
                    if ( i & ( 1 << j ) ) {
                        so_part = so_part & lut_in( j );
                    } else {
                        so_part = so_part & ( ~lut_in( j ) );
                    }
                }

                add_o6_so = add_o6_so | so_part;
            }
        }

        lut_init lut_add3( add_o5_co, add_o6_so );
        return lut_add3.get_hex();
    }

    void Xilinx_TernaryAdd_2State::insertCarryInit() {
        int bitmask_ccount = 0, bitmask2_ccount = 0;

        for( int i = 0; i < 3; i++ ) {
            if( bitmask_ & ( 1 << i ) ) {
                bitmask_ccount++;
            }

            if( bitmask2_ & ( 1 << i ) ) {
                bitmask2_ccount++;
            }
        }

        string carry_cct, bbus;

        if( abs( bitmask_ccount - bitmask2_ccount ) == 0 ) {
            switch( bitmask2_ccount ) {
                case 0:
                    carry_cct = "'0'";
                    bbus = "'0'";
                    break;

                case 1:
                    carry_cct = "'0'";
                    bbus = "'1'";
                    break;

                case 2:
                    carry_cct = "'1'";
                    bbus = "'1'";
                    break;
            }
        } else if( abs( bitmask_ccount - bitmask2_ccount ) == 1 ) {
            if( bitmask_ccount == 1 ) {
                if( bitmask2_ccount == 2 ) {
                    carry_cct = "'1'";
                    bbus = "sel_i";
                } else {
                    carry_cct = "'0'";
                    bbus = "not sel_i";
                }
            } else if( bitmask_ccount == 0 ) {
                carry_cct = "'0'";
                bbus = "sel_i";
            } else {
                carry_cct = "'1'";
                bbus = "not sel_i";
            }
        } else if( abs( bitmask_ccount - bitmask2_ccount ) == 2 ) {
            if( bitmask_ccount == 0 ) {
                carry_cct = "sel_i";
                bbus = "sel_i";
            } else {
                carry_cct = "not sel_i";
                bbus = "not sel_i";
            }
        }

        if( carry_cct.empty() || bbus.empty() ) {
            throw "No carry init found";
        }

        vhdl << "\tcarry_cct <= " << carry_cct <<  ";" << endl;
        vhdl << "\tbbus(0) <= "  <<  bbus  <<  ";"  << endl;
    }

    void Xilinx_TernaryAdd_2State::computeState() {
        if( bitmask_ == bitmask2_ ) {
            bitmask2_ = -1;
        }

        short states[] = {bitmask_, bitmask2_};
        bool lock[] = {false, false, false};
        stringstream debug;
        int k_max = 2;

        if( bitmask2_ == -1 ) {
            k_max = 1;
        }

        mapping[0] = 0;
        mapping[1] = 1;
        mapping[2] = 2;

        if( states[0] & 0x4 ) {
            lock[2] = true;
        }

        for( int k = 0; k < k_max; k++ ) {
            for( int i = 2; i >= 0; --i ) {
                if( states[k] & ( 1 << mapping[i] ) ) {
                    for( int j = i; j < 2; ++j ) {
                        if( !lock[j + 1] ) {
                            short t      = mapping[j + 1];
                            mapping[j + 1] = mapping[j];
                            mapping[j]   = t;
                        } else {
                            break;
                        }
                    }
                }
            }

            for( int i = 2; i >= 0; --i ) {
                if( states[k] & ( 1 << mapping[i] ) ) {
                    lock[i] = true;
                }
            }
        }

        debug << "Ternary_2State with states {" << bitmask_ << "," << bitmask2_ << "}" << endl;
        short states_postmap[] = {0, 0};

        for( int k = 0; k < k_max; k++ ) {
            for( int i = 0; i < 3; i++ ) {
                if( states[k] & ( 1 << mapping[i] ) ) {
                    states_postmap[k] |= ( 1 << i );
                }
            }
        }

        if( bitmask2_ == -1 ) {
            states_postmap[1] = -1;
        } else {
            if( ( states_postmap[0] & 0x6 ) == 6 && ( states_postmap[1] & 0x6 ) == 2 ) {
                short t      = mapping[1];
                mapping[1] = mapping[2];
                mapping[2]   = t;
                states_postmap[1] = ( states_postmap[1] & 0x1 ) + 4;
            }
        }

        for( int i = 0; i < 3; i++ ) {
            debug << "map " << i << " to " << mapping[i] << endl;
        }

        debug << "postmap states {" << states_postmap[0] << "," << states_postmap[1] << "}" << endl;
        state_type = 0;

        if( states_postmap[0] == 0 ) {
            if      ( states_postmap[1] == 4 ) {
                state_type = 3;
            } else if ( states_postmap[1] == 6 ) {
                state_type = 5;
            } else if ( states_postmap[1] == -1 ) {
                state_type = 0;
            }
        } else if ( states_postmap[0] == 4 ) {
            if      ( states_postmap[1] == 2 ) {
                state_type = 4;
            } else if ( states_postmap[1] == 6 ) {
                state_type = 6;
            } else if ( states_postmap[1] == 3 ) {
                state_type = 7;
            } else if ( states_postmap[1] == 0 ) {
                state_type = 3;
            } else if ( states_postmap[1] == -1 ) {
                state_type = 1;
            }
        } else if ( states_postmap[0] == 6 ) {
            if      ( states_postmap[1] == 5 ) {
                state_type = 8;
            } else if ( states_postmap[1] == 1 ) {
                state_type = 7;
            } else if ( states_postmap[1] == 0 ) {
                state_type = 5;
            } else if ( states_postmap[1] == 4 ) {
                state_type = 6;
            } else if ( states_postmap[1] == -1 ) {
                state_type = 2;
            }
        }

        debug << "found type " << state_type << endl;
        //cerr << debug.str();
        REPORT( DEBUG, debug.str() );

        if( bitmask_ > 0 && state_type == 0 ) {
            throw "2State type not valid";
        }
        
    }
	OperatorPtr Xilinx_TernaryAdd_2State::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args ){
		if( target->getVendor() != "Xilinx" )
			throw std::runtime_error( "Can't build xilinx primitive on non xilinx target" );

		int wIn;
		int bitmask,bitmask2;
		UserInterface::parseInt(args,"wIn",&wIn );
		UserInterface::parseInt(args,"bitmask",&bitmask);
		UserInterface::parseInt(args,"bitmask2",&bitmask2);
		return new Xilinx_TernaryAdd_2State(parentOp,target,wIn,bitmask,bitmask2);
	}

	void Xilinx_TernaryAdd_2State::registerFactory(){
		 UserInterface::add( "XilinxTernaryAddSub", // name
                             "A ternary adder subtractor build of xilinx primitives.", // description, string
                             "Primitives", // category, from the list defined in UserInterface.cpp
                             "",
                             "wIn(int): The wordsize of the adder; \
                             bitmask(int)=0: First bitmask for input negation; \
                             bitmask2(int)=-1: Second bitmask for configurable input negation;",
                             "",
                             Xilinx_TernaryAdd_2State::parseArguments,
							 Xilinx_TernaryAdd_2State::unitTest
		 );
	 }

	void Xilinx_TernaryAdd_2State::emulate(TestCase *tc){
		mpz_class x = tc->getInputValue("x_i");
		mpz_class y = tc->getInputValue("y_i");
		mpz_class z = tc->getInputValue("z_i");
		mpz_class s = 0;
		mpz_class sel = 0;
		if( bitmask_ != bitmask2_  ){
			sel = tc->getInputValue("sel_i");
		}
		short signs = 0;
		if(sel==0){
			signs = bitmask_;
		}else{
			signs = bitmask2_;
		}

		if(0x1&signs)
			s -= x;
		else
			s += x;

		if(0x2&signs)
			s -= y;
		else
			s += y;

		if(0x4&signs)
			s -= z;
		else
			s += z;


		mpz_class one = 1;

		mpz_class mask = ((one<<wIn_)-1);
		s = s & mask;

		tc->addExpectedOutput("sum_o", s);
	}

	TestList Xilinx_TernaryAdd_2State::unitTest(int index)
	{
        TestList testStateList;
        vector<pair<string,string>> paramList;

        if(index==-1)
        { // The unit tests
            for(int w=2; w <= 32; w++)
            {
                paramList.push_back(make_pair("wIn", to_string(w)));
                testStateList.push_back(paramList);
                paramList.clear();
            }
        }
        else
        {
            // finite number of random test computed out of index
        }

        return testStateList;
	}

}//namespace
