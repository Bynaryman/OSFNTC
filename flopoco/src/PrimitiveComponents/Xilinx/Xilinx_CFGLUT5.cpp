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
#include "Xilinx_CFGLUT5.hpp"

using namespace std;
namespace flopoco {
	Xilinx_CFGLUT5::Xilinx_CFGLUT5(Operator *parentOp, Target* target) : Primitive(parentOp,target)
	{
        //isSequential_= false;
        //hasRegistersWithoutReset_

		srcFileName="Xilinx_CFGLUT5";
        setNameWithFreqAndUID("CFGLUT5");
        //setCopyrightString("ACME and Co 2010");


		// declaring inputs
        addInput("clk");//clk
        addInput("ce"); //clk enable
        addInput("cdi");//configuration data in
		
		//selector inputs
        addInput("i0");
        addInput("i1");
        addInput("i2");
        addInput("i3");
        addInput("i4");
		
		// declaring output
        addOutput("o5"); //4 LUT output
        addOutput("o6"); //5 LUT output
        addOutput("cdo"); //configuration data out

        ///addComment(" This is a primitive and will directly be maped to a LUT without any logic description ");

		// basic message
		REPORT(INFO,"Declaration of Xilinx_CFGLUT5 \n");
		// more detailed message
		REPORT(DETAILED, "this operator has received no parameters ");
		// debug message for developper
		REPORT(DEBUG,"debug of Xilinx_CFGLUT5");
	};

	
	void Xilinx_CFGLUT5::emulate(TestCase * tc) {

	}

	void Xilinx_CFGLUT5::buildStandardTestCases(TestCaseList * tcl) {
		// please fill me with regression tests or corner case tests!
	}

	OperatorPtr Xilinx_CFGLUT5::parseArguments(Target *target, vector<string> &args) {
		return new Xilinx_CFGLUT5(target);
	}
	
	void Xilinx_CFGLUT5::registerFactory(){
		UserInterface::add("Xilinx_CFGLUT5", // name
											 "My first Xilinx_CFGLUT5.", // description, string
											 "NeuralNetworks", // category, from the list defined in UserInterface.cpp
											 "", //seeAlso
											 // Now comes the parameter description string.
											 // Respect its syntax because it will be used to generate the parser and the docs
											 // Syntax is: a semicolon-separated list of parameterDescription;
											 // where parameterDescription is parameterName (parameterType)[=defaultValue]: parameterDescriptionString 
											 "",
											 // More documentation for the HTML pages. If you want to link to your blog, it is here.
											 "Feel free to experiment with its code, it will not break anything in FloPoCo. <br> Also see the developper manual in the doc/ directory of FloPoCo.",
											 Xilinx_CFGLUT5::parseArguments
											 ) ;
	}


}//namespace
