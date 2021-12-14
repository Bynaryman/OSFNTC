#include <iostream>
#include <sstream>

#include "gmp.h"
#include "mpfr.h"
#include "GenericMult.hpp"
#include "Primitive.hpp"

using namespace std;
namespace flopoco {
    GenericMult::GenericMult(Target* target, const int &wX, const int &wY) : Operator(target), flags_(wY) {
        setCopyrightString( UniKs::getAuthorsString( UniKs::AUTHOR_MKLEINLEIN ) );
        srcFileName="GenericMult";
		ostringstream name;
        name << "GenericMult_w" << wX << "_" << printFlags();
        setNameWithFreqAndUID(name.str());

        this->wX = wX;
        this->wY = wY;

        this->wR = wX+wY;

        addInput("X",wX);
        addInput("Y",wY);

        addOutput("R",wR);

        if( target->useTargetOptimizations() && target->getVendor() == "Xilinx" )
            buildXilinx(target,wX);
        else if( target->useTargetOptimizations() && target->getVendor()=="Altera" )
            buildAltera(target,wX);
        else
            buildCommon(target,wX);

    }

    void GenericMult::buildXilinx(Target* target, const uint32_t &wIn){
        REPORT(LIST,"Xilinx junction not fully implemented, fall back to common.");
        buildCommon(target,wIn);

    }

    void GenericMult::buildAltera(Target *target, const uint32_t &wIn){
        REPORT(LIST,"Altera junction not fully implemented, fall back to common.");
        buildCommon(target,wIn);

    }

    void GenericMult::buildCommon(Target* target, const uint32_t &wIn){
        const uint16_t c_count = (hasFlags(CONF_LEFT)?1:0) + (hasFlags(CONF_RIGHT)?1:0) + (hasFlags(TERNARY|CONF_MID)?1:0);
        if(c_count >0){
            vhdl << declare( "CONF", c_count ) << " <= ";
            if( hasFlags(CONF_LEFT) ){
                vhdl << "iL_c";
                if( hasFlags(TERNARY|CONF_MID) ){
                    vhdl << "& iM_c";
                }
                if( hasFlags(CONF_RIGHT) ){
                    vhdl << "& iR_c";
                }
            }else if( hasFlags(TERNARY|CONF_MID) ){
                vhdl << "iM_c";
                if( hasFlags(CONF_RIGHT) ){
                    vhdl << "& iM_c";
                }
            }else if( hasFlags(CONF_RIGHT) ){
                vhdl << "& iM_c";
            }
            vhdl << std::endl;
            vhdl << "case CONF is" << std::endl;
            for( int i = 0;i<(1<<c_count);++i ){
                vhdl << "\t" << "when \"";
                for( int j=c_count-1;j>=0;--j )
                    vhdl << (i&(1<<j)?"1":"0");
                vhdl << "\"\t=> std_logic_vector(";
                uint16_t mask = 1<<(c_count-1);
                if( hasFlags(CONF_LEFT) ){
                    vhdl << (i&(mask)?"-":"+") << "unsigned(iL)";
                    mask >>= 1;
                    if( hasFlags(TERNARY|CONF_MID) ){
                        vhdl << (i&mask?"-":"+") << "unsigned(iM)";
                        mask >>= 1;
                    }
                    if( hasFlags(CONF_RIGHT) ){
                        vhdl << (i&mask?"-":"+") << "unsigned(iR)";
                    }
                }else if( hasFlags(TERNARY|CONF_MID) ){
                    vhdl << "unsigned(iL)";
                    vhdl << (i&mask?"-":"+") << "unsigned(iM)";
                    mask >>= 1;
                    if( hasFlags(CONF_RIGHT) ){
                        vhdl << (i&mask?"-":"+") << "unsigned(iR)";
                    }
                }else if( hasFlags(CONF_RIGHT) ){
                    vhdl << (i&mask?"-":"+") << "unsigned(iR)";
                }
                vhdl << ");" << std::endl;
            }
            vhdl << "\t" << "when others => oSum <= (others=>'X');" << std::endl;
            vhdl << "end case;" << std::endl;
        }
    }

    void GenericMult::emulate(TestCase * tc) {

	}


    void GenericMult::buildStandardTestCases(TestCaseList * tcl) {
		// please fill me with regression tests or corner case tests!
	}

}//namespace
