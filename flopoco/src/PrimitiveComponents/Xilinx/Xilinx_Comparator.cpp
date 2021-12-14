#include "Xilinx_Comparator.hpp"
#include "Xilinx_CARRY4.hpp"

namespace flopoco {
	Xilinx_Comparator::Xilinx_Comparator(Operator *parentOp, Target *target, int wIn, ComparatorType type ) : Operator( parentOp,target ) , m_type( type ) {
        setCopyrightString( "Marco Kleinlein" );

        std::stringstream name_str;
        name_str << "Xilinx_Comparator_";

        switch( type ) {
            case flopoco::Xilinx_Comparator::ComparatorType_invalid:
                break;

            case ComparatorType_gt:
                name_str << "gt_";
                break;

            case ComparatorType_lt:
                name_str << "lt_";
                break;

            case ComparatorType_ge:
                name_str << "ge_";
                break;

            case ComparatorType_le:
                name_str << "le_";
                break;

            case ComparatorType_eq:
                name_str << "eq_";
                break;

            case ComparatorType_ne:
                name_str << "ne_";
                break;
        }

        name_str << wIn;
        setNameWithFreqAndUID( name_str.str() );
        setCombinatorial();
        REPORT( DEBUG , "Building" + this->getName() );
        srcFileName = "Xilinx_Comparator";

        int needed_luts = ( wIn >> 1 );
        int ws_remain = wIn % 2;
        int needed_cc = ( wIn / 8 ) + ( wIn % 8 > 0 ? 1 : 0 );
        addInput( "a", wIn );
        addInput( "b", wIn );
        addOutput( "o", 1 , 2,false );
        declare( "cc_s", needed_cc * 4 );
        declare( "cc_di", needed_cc * 4 );
        declare( "cc_co", needed_cc * 4 );

        if( ( needed_cc * 4 ) > wIn ) {
            const int start = needed_cc * 4 - 1;
            const int stop = needed_luts + ( ws_remain ? 1 : 0 );

            if( ( start - stop ) == 0 ) {
                vhdl << tab << "cc_s <= '0'";
                vhdl << tab << "cc_di <= '0'";
                vhdl << tab << "cc_co <= '0'";
            } else {
                vhdl << tab << "cc_s <= (others => '0');";
                vhdl << tab << "cc_di <= (others => '0');";
                vhdl << tab << "cc_co <= (others => '0');";
            }
        }

        bool c_init = false;
        string lut_content;

        switch( type ) {
            case flopoco::Xilinx_Comparator::ComparatorType_invalid:
                break;

            case ComparatorType_gt:
                lut_content = getLUT_gt();
                break;

            case ComparatorType_lt:
                lut_content = getLUT_lt();
                break;

            case ComparatorType_ge:
                lut_content = getLUT_ge();
                c_init = true;
                break;

            case ComparatorType_le:
                lut_content = getLUT_le();
                c_init = true;
                break;

            case ComparatorType_eq:
                lut_content = getLUT_eq();
                c_init = true;
                break;

            case ComparatorType_ne:
                lut_content = getLUT_ne();
                break;
        }

        for( int i = 0; i < needed_luts; i++ ) {
			Xilinx_LUT6_2 *cur_lut = new Xilinx_LUT6_2( parentOp,target );
            cur_lut->setGeneric( "init", lut_content, 64 );
            inPortMap("i0", "a" + of( i * 2 ) );
						inPortMap("i1", "a" + of( i * 2 + 1 ) );
						inPortMap("i2", "b" + of( i * 2 ) );
						inPortMap("i3", "b" + of( i * 2 + 1 ) );
            inPortMapCst("i4", "'0'" );
						inPortMapCst("i5", "'1'" );
            outPortMap("o6", "cc_s" + of( i ));
						outPortMap("o5", "cc_di" + of( i ));
            stringstream lut_name;
            lut_name << "lut_" << i;
            vhdl << cur_lut->primitiveInstance( lut_name.str() ) << endl;
        }

        if( ws_remain ) {
			Xilinx_LUT6_2 *cur_lut = new Xilinx_LUT6_2( parentOp,target );
            cur_lut->setGeneric( "init", lut_content, 64 );
            inPortMap("i0", "a" + of( needed_luts * 2 ) );
						inPortMap("i1", "a" + of( needed_luts * 2 ) );
						inPortMap("i2", "b" + of( needed_luts * 2 ) );
						inPortMap("i3", "b" + of( needed_luts * 2 ) );
            inPortMapCst( "i4", "'0'" );
            inPortMapCst( "i5", "'1'" );
            outPortMap("o6", "cc_s" + of( needed_luts ));
						outPortMap("o5", "cc_di" + of( needed_luts ));
            stringstream lut_name;
            lut_name << "hlut";
            vhdl << cur_lut->primitiveInstance( lut_name.str() ) << endl;
        }

        for( int i = 0; i < needed_cc; i++ ) {
			Xilinx_CARRY4 *cur_cc = new Xilinx_CARRY4( parentOp,target );
            if( i == 0 ) {
                inPortMapCst( "cyinit", ( c_init ? "'1'" : "'0'" ) );
                inPortMapCst( "ci", "'0'" );
            } else {
                inPortMapCst( "cyinit", "'0'" );
                inPortMap( "ci", "cc_co" + of( i * 4 - 1 ) );
            }

            inPortMap( "di", "cc_di" + range( i * 4 + 3, i * 4 ) );
            inPortMap( "s", "cc_s" + range( i * 4 + 3, i * 4 ) );
            outPortMap( "co", "cc_co" + range( i * 4 + 3, i * 4 ));
            stringstream cc_name;
            cc_name << "cc_" << i;
            vhdl << cur_cc->primitiveInstance( cc_name.str() );
        }

        vhdl << tab << "o <= cc_co" << of( needed_luts + ( ws_remain ? 1 : 0 ) - 1 ) << ";" << std::endl;
    }

    void Xilinx_Comparator::emulate( TestCase *tc ) {
        mpz_class a = tc->getInputValue( "a" );
        mpz_class b = tc->getInputValue( "b" );
        mpz_class expectedResult;

        switch( m_type ) {
            case flopoco::Xilinx_Comparator::ComparatorType_invalid:
                break;

            case ComparatorType_gt:
                expectedResult = ( a > b );
                break;

            case ComparatorType_lt:
                expectedResult = ( a < b );
                break;

            case ComparatorType_ge:
                expectedResult = ( a >= b );
                break;

            case ComparatorType_le:
                expectedResult = ( a <= b );
                break;

            case ComparatorType_eq:
                expectedResult = ( a == b );
                break;

            case ComparatorType_ne:
                expectedResult = ( a != b );
                break;
        }

        tc->addExpectedOutput( "o", expectedResult );
    }

    void Xilinx_Comparator::buildStandardTestCases( TestCaseList *tcl ) {
        for( int c = 0; c < 5; c++ ) {
            for( int i = 0; i < 5; i++ ) {
                TestCase *tc = new TestCase( this );
                mpz_class a = c;
                mpz_class b = i;
                tc->setInputValue( "a", a );
                tc->setInputValue( "b", b );
                mpz_class expectedResult;

                switch( m_type ) {
                    case flopoco::Xilinx_Comparator::ComparatorType_invalid:
                        break;

                    case ComparatorType_gt:
                        expectedResult = ( a > b );
                        break;

                    case ComparatorType_lt:
                        expectedResult = ( a < b );
                        break;

                    case ComparatorType_ge:
                        expectedResult = ( a >= b );
                        break;

                    case ComparatorType_le:
                        expectedResult = ( a <= b );
                        break;

                    case ComparatorType_eq:
                        expectedResult = ( a == b );
                        break;

                    case ComparatorType_ne:
                        expectedResult = ( a != b );
                        break;
                }

                tc->addExpectedOutput( "o", expectedResult );
                tcl->add( tc );
            }
        }
    }

    string Xilinx_Comparator::getLUT_lt() { // a < b
        lut_op op_o6 = ~( lut_in( 0 )^lut_in( 2 ) ) & ~( lut_in( 1 )^lut_in( 3 ) );
        lut_op op_o5 = ( ~( lut_in( 1 ) )&lut_in( 3 ) )
                       | ( ~( lut_in( 1 )^lut_in( 3 ) ) & ( ~( lut_in( 0 ) )&lut_in( 2 ) ) );
        lut_init op( op_o5, op_o6 );
        return op.get_hex();
    }

    string Xilinx_Comparator::getLUT_gt() { // a > b
        lut_op op_o6 = ~( lut_in( 0 )^lut_in( 2 ) ) & ~( lut_in( 1 )^lut_in( 3 ) );
        lut_op op_o5 = ( ~( lut_in( 3 ) )&lut_in( 1 ) )
                       | ( ~( lut_in( 3 )^lut_in( 1 ) ) & ( ~( lut_in( 2 ) )&lut_in( 0 ) ) );
        lut_init op( op_o5, op_o6 );
        return op.get_hex();
    }

    string Xilinx_Comparator::getLUT_le() { // a <= b
        lut_op op_o6 = ~( lut_in( 0 )^lut_in( 2 ) ) & ~( lut_in( 1 )^lut_in( 3 ) );
        lut_op op_o5 = ( ~( lut_in( 1 ) )&lut_in( 3 ) )
                       | ( ~( lut_in( 1 )^lut_in( 3 ) ) & ( ~( lut_in( 0 ) )&lut_in( 2 ) ) )
                       | ( ~( lut_in( 0 )^lut_in( 2 ) ) & ~( lut_in( 1 )^lut_in( 3 ) ) );
        lut_init op( op_o5, op_o6 );
        return op.get_hex();
    }

    string Xilinx_Comparator::getLUT_ge() { // a >= b
        lut_op op_o6 = ~( lut_in( 0 )^lut_in( 2 ) ) & ~( lut_in( 1 )^lut_in( 3 ) );
        lut_op op_o5 = ( ~( lut_in( 3 ) )&lut_in( 1 ) )
                       | ( ~( lut_in( 3 )^lut_in( 1 ) ) & ( ~( lut_in( 2 ) )&lut_in( 0 ) ) )
                       | ( ~( lut_in( 0 )^lut_in( 2 ) ) & ~( lut_in( 1 )^lut_in( 3 ) ) );
        lut_init op( op_o5, op_o6 );
        return op.get_hex();
    }

    string Xilinx_Comparator::getLUT_eq() { // a = b
        lut_op op_o6 = ~( lut_in( 0 )^lut_in( 2 ) ) & ~( lut_in( 1 )^lut_in( 3 ) );
        lut_op op_o5 = ~( lut_in( 0 )^lut_in( 2 ) ) & ~( lut_in( 1 )^lut_in( 3 ) );
        lut_init op( op_o5, op_o6 );
        return op.get_hex();
    }

    string Xilinx_Comparator::getLUT_ne() { // a != b
        lut_op op_o6 = ~( lut_in( 0 )^lut_in( 2 ) ) & ~( lut_in( 1 )^lut_in( 3 ) );
        lut_op op_o5 = ( lut_in( 0 )^lut_in( 2 ) ) | ( lut_in( 1 )^lut_in( 3 ) );
        lut_init op( op_o5, op_o6 );
        return op.get_hex();
    }

    Xilinx_Comparator::ComparatorType Xilinx_Comparator::getTypeFromString( const string &typestr ) {
        if( typestr == "gt" ) {
            return ComparatorType_gt;
        } else if ( typestr == "ge" ) {
            return ComparatorType_ge;
        } else if ( typestr == "lt" ) {
            return ComparatorType_lt;
        } else if( typestr == "le" ) {
            return ComparatorType_le;
        } else if( typestr == "eq" ) {
            return ComparatorType_eq;
        } else if( typestr == "ne" ) {
            return ComparatorType_ne;
        } else {
            return ComparatorType_invalid;
        }
    }

    OperatorPtr Xilinx_Comparator::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args )
    {
        int wIn;
        std::string typestr;
        UserInterface::parseInt( args, "wIn", &wIn );
        UserInterface::parseString( args, "type", &typestr );
        Xilinx_Comparator::ComparatorType type = Xilinx_Comparator::getTypeFromString( typestr );

        if( type == ComparatorType_invalid ) {
            std::cerr << "Comparator type invalid." << std::endl;
            exit( -1 );
        }

		return new Xilinx_Comparator( parentOp, target, wIn, type );
    }

    void Xilinx_Comparator::registerFactory() {
        UserInterface::add( "XilinxComparator", // name
                            "A comparator build of xilinx primitives.", // description, string
                            "Primitives", // category, from the list defined in UserInterface.cpp
                            "", //seeAlso
                            // where parameterDescription is parameterName (parameterType)[=defaultValue]: parameterDescriptionString
                            "wIn (int): Wordsize of comparator inputs; \
                             type (string): Type of comparator ( gt,ge,lt,le,eq,ne )",
                            "Nope.",
                            Xilinx_Comparator::parseArguments
                          ) ;
    }

}
