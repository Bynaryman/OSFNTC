#include <iostream>
#include <sstream>

#include "gmp.h"
#include "mpfr.h"
#include "GenericMux.hpp"
#include "Primitive.hpp"

#include "Xilinx/Xilinx_GenericMux.hpp"

using namespace std;
namespace flopoco {
    GenericMux::GenericMux(Operator* parentOp, Target* target, const uint32_t &wIn, const uint32_t &inputCount) : Operator(parentOp,target) {
        setCopyrightString( "Marco Kleinlein" );
        srcFileName="GenericMux";
		ostringstream name;
        name << "GenericMux_w" << wIn << "_s" << inputCount;
        setNameWithFreqAndUID(name.str());

        for( uint i=0;i<inputCount;++i )
            addInput(getInputName(i),wIn);

        addInput(getSelectName(), intlog2( inputCount-1 ) );
        addOutput(getOutputName(),wIn);

        if( target->useTargetOptimizations() && target->getVendor() == "Xilinx" )
            buildXilinx(parentOp, target,wIn,inputCount);
        else if( target->useTargetOptimizations() && target->getVendor()=="Altera" )
            buildAltera(parentOp, target,wIn,inputCount);
        else
            buildCommon(target,wIn,inputCount);

    }

    void GenericMux::buildXilinx(Operator* parentOp, Target* target, const uint32_t &wIn, const uint32_t &inputCount){
        Xilinx_GenericMux *mux = new Xilinx_GenericMux(parentOp, target,inputCount,wIn );
        inPortMap("s_in",getSelectName());
        for( uint i=0;i<inputCount;++i )
            inPortMap(join( "x", i, "_in" ), getInputName(i));

        outPortMap("x_out",getOutputName() );
    }

    void GenericMux::buildAltera(Operator* parentOp, Target *target, const uint32_t &wIn, const uint32_t &inputCount){
        REPORT(LIST,"Altera junction not fully implemented, fall back to common.");
        buildCommon(target,wIn,inputCount);
    }

    void GenericMux::buildCommon(Target* target, const uint32_t &wIn, const uint32_t &inputCount){
        const uint16_t select_ws = intlog2( inputCount-1 );

        vhdl << tab << "with " << getSelectName() << " select" << endl
             << tab << tab << getOutputName() << " <= " << endl;
        for(unsigned int i=0; i< inputCount; i++)
        {
            vhdl << tab << tab << tab << getInputName(i) << " when \"";
            cout << "GenericMux: getInputName(" << i << ")=" << getInputName(i) << endl;
            for(int j=select_ws-1; j>=0; --j)
            {
                vhdl << (i&(1<<j)?"1":"0");
            }
            vhdl << "\"," << endl;
        }
        vhdl << "(others=>'X') when others;" << endl;
    }

    std::string GenericMux::getSelectName() const{
        return "iSel";
    }

    std::string GenericMux::getInputName(const uint32_t &index) const{
        std::stringstream o;
        o << "iS_" << index;
        return o.str();
    }

    std::string GenericMux::getOutputName() const{
        return "oMux";
    }

    void GenericMux::emulate(TestCase * tc) {

	}


    void GenericMux::buildStandardTestCases(TestCaseList * tcl) {
		// please fill me with regression tests or corner case tests!
	}

  OperatorPtr GenericMux::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
    int wIn, inputCount;
    UserInterface::parseStrictlyPositiveInt(args, "wIn", &wIn);
    UserInterface::parseStrictlyPositiveInt(args, "inputCount", &inputCount);
    return new GenericMux(parentOp, target, (unsigned int) wIn, (unsigned int) inputCount);
  }

  void GenericMux::registerFactory() {
    UserInterface::add("GenericMux",
    "A Multiplexer",
    "ShiftersLZOCs",
    "",
    "wIn(int): input word size; \
    inputCount(int): the number of data inputs (NOT counting the select input!)",
    "",
    GenericMux::parseArguments);
  }


}//namespace
