// general c++ library for manipulating streams
#include <iostream>
#include <sstream>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitrary large
   entries */
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "FixComplexAdder.hpp"

using namespace std;
namespace flopoco {

       FixComplexAdder::~FixComplexAdder(){};


	FixComplexAdder::FixComplexAdder(OperatorPtr parentOp, Target* target, int msbin_, int lsbin_, int msbout_, int lsbout_, bool signedIn_, bool addorsub_, bool laststage_) :
	Operator(parentOp, target), msbin(msbin_), lsbin(lsbin_), msbout(msbout_), lsbout(lsbout_), signedIn(signedIn_), addorsub(addorsub_), laststage(laststage_)
	{

		// definition of the source file name, used for info and error reporting using REPORT 
		srcFileName="FixComplexAdder";

		// definition of the name of the operator

		ostringstream name;
		name << "FixComplexAdder_" << msbin << "_" << abs(lsbin);
		setNameWithFreqAndUID( name.str() );
		//setName(name.str()); // See also setNameWithFrequencyAndUID()
		// Copyright 
		setCopyrightString("Hatam Abdoli & Florent de Dinechin");

		/* SET UP THE IO SIGNALS
		   Each IO signal is declared by addInput(name,n) or addOutput(name,n) 
		   where name is a string that stands for the name of the variable and 
		   n is an integer (int)   that stands for the length of the corresponding 
		   input/output */

		int input_width = msbin-lsbin+1;
		int output_width = msbout-lsbout+1;
		int lsb_diff = ( (lsbout<lsbin) ? (-(lsbout - lsbin)) : 0);
		int g = lsbout - lsbin;


		// declaring inputs
		addInput ("Xr" , input_width);
		addInput ("Xi" , input_width);
		addInput ("Yr" , input_width);
		addInput ("Yi" , input_width);
		// declaring output
		addOutput("Sr" , output_width);
		addOutput("Si" , output_width);

		
		addFullComment("Start of vhdl generation"); // this will be a large, centered comment in the VHDL



		/* Some piece of information can be delivered to the flopoco user if  the -verbose option is set
		   [eg: flopoco -verbose=0 TutorialOperator 10 5 ]
		   , by using the REPORT function.
		   There is three level of details
		   -> INFO for basic information ( -verbose=1 )
		   -> DETAILED for complete information for the user of the operator (includes the INFO level) ( -verbose=2 )
		   -> DEBUG for complete and debug information, to be used during operator development (includes the INFO and DETAILED levels) ( -verbose=3 )
		*/
		// basic message
		REPORT(INFO,"Declaration of FixComplexAdder \n");

		// more detailed message
		REPORT(DETAILED, "this operator has received Six parameters " << msbin << " and " << lsbin << " and " << msbout << " and " << lsbout << " and " << signedIn << " and " << addorsub);
  
		// debug message for developer
		REPORT(DEBUG,"debug of FixComplexAdder");


		/* vhdl is the stream which receives all the vhdl code, some special functions are
		   available to smooth variable declaration and use ... 
		   -> when first using a variable (Eg: T), declare("T",64) will create a vhdl
		   definition of the variable : signal T and includes it it the header of the architecture definition of the operator

		   Each code transmitted to vhdl will be parsed and the variables previously declared in a previous cycle will be delayed automatically by a pipelined register.
		*/
//		cout << endl << "msbin=" << std::to_string(msbin) << " lsbin=" + std::to_string(lsbin) << 
//							" msbout=" << std::to_string(msbout) << " lsbout=" << std::to_string(lsbout) << " laststage=" << std::to_string(laststage) << endl; 

		if(signedIn)
		{
			if(lsbout < lsbin)
			{
				vhdl << tab << "Sr" << range(lsb_diff-1, 0) << " <= " << zg(lsb_diff) << ";" << endl;
				vhdl << tab << "Si" << range(lsb_diff-1, 0) << " <= " << zg(lsb_diff) << ";" << endl;
			}

			if (addorsub) //Addition
			{

				if(!laststage)
				{
					vhdl <<tab<< declare(
													getTarget()->adderDelay(input_width+1),  // contribution to the critical path of this VHDL operation
													"Tr",                                // signal name (will be returned by declare())
													input_width+1)                           // signal size, in bits
							 << " <= (Xr" << of(msbin-lsbin) << " & Xr) + (Yr" << of(msbin-lsbin) << " & Yr);" << endl;
					vhdl << tab << "Sr" <<  range(input_width+lsb_diff,lsb_diff) << "<= Tr" << range(input_width,0) << ";" << endl;


					vhdl <<tab<< declare(
													getTarget()->adderDelay(input_width+1),  // contribution to the critical path of this VHDL operation
													"Ti",                                // signal name (will be returned by declare() )
													input_width+1)                           // signal size, in bits
							 << " <= (Xi" << of(msbin-lsbin) << " & Xi) + (Yi" << of(msbin-lsbin) << " & Yi);" << endl;
					vhdl << tab << "Si" <<  range(input_width+lsb_diff,lsb_diff) << "<= Ti" << range(input_width,0) << ";" << endl;

				}
				else	// Last stage of FFT
				{
					if (g == 0)  // The topmost butterfly, without multiplication
					{
						vhdl <<tab<< declare(
										getTarget()->adderDelay(output_width),  // contribution to the critical path of this VHDL operation
										"Tr",                                // signal name (will be returned by declare())
										output_width)                           // signal size, in bits
								 << " <= (Xr" << of(msbin-lsbin) << " & Xr) + (Yr" << of(msbin-lsbin) << " & Yr);" << endl;
						vhdl << tab << "Sr <= Tr;" << endl;
						
						vhdl <<tab<< declare(
										getTarget()->adderDelay(output_width),  // contribution to the critical path of this VHDL operation
										"Ti",                                // signal name (will be returned by declare())
										output_width)                           // signal size, in bits
							 << " <= (Xi" << of(msbin-lsbin) << " & Xi) + (Yi" << of(msbin-lsbin) << " & Yi);" << endl;
						vhdl << tab << "Si <= Ti;" << endl;
					}
					else
					{
						vhdl <<tab<< declare(
										getTarget()->adderDelay(output_width+1),  // contribution to the critical path of this VHDL operation
										"Tr",                                // signal name (will be returned by declare())
										output_width+1)                           // signal size, in bits
										 << " <= (Xr" << of(msbin-lsbin) << " & Xr" << range(input_width-1,g-1) << ") + (Yr" << of(msbin-lsbin) << " & Yr"  << 										range(input_width-1,g-1) << ") + (" << zg(output_width)  << " & \"1\" );" << endl;

						vhdl << tab << "Sr" <<  range(output_width-1,0) << "<= Tr" << range(output_width,1) << ";" << endl;

	//					vhdl <<tab<< declare(
	//									getTarget()->adderDelay(output_width+1),  // contribution to the critical path of this VHDL operation
	//									"Tr1",                                // signal name (will be returned by declare())
	//									output_width+1)                           // signal size, in bits
	//									 << " <= (Xr" << of(msbin-lsbin) << " & Xr" << range(input_width-1,g-1) << ") + (Yr" << of(msbin-lsbin) << " & Yr"  << 										range(input_width-1,g-1) << ");" << endl;

	//					vhdl <<tab<< declare(
	//									getTarget()->adderDelay(output_width+1),  // contribution to the critical path of this VHDL operation
	//									"Tr2",                                // signal name (will be returned by declare())
	//									output_width+1)                           // signal size, in bits
	//									 << " <= Tr1 + (" << zg(output_width)  << " & \"1\" );" << endl;
	//
	//					vhdl << tab << "Sr" <<  range(output_width-1,0) << "<= Tr2" << range(output_width,1) << ";" << endl;

						// Imaginary part
						vhdl <<tab<< declare(
										getTarget()->adderDelay(output_width+1),  // contribution to the critical path of this VHDL operation
										"Ti",                                // signal name (will be returned by declare())
										output_width+1)                           // signal size, in bits
										 << " <= (Xi" << of(msbin-lsbin) << " & Xi" << range(input_width-1,g-1) << ") + (Yi" << of(msbin-lsbin) << " & Yi"  << 										range(input_width-1,g-1) << ") + (" << zg(output_width)  << " & \"1\" );" << endl;

						vhdl << tab << "Si" <<  range(output_width-1,0) << "<= Ti" << range(output_width,1) << ";" << endl;

	//					vhdl <<tab<< declare(
	//									getTarget()->adderDelay(output_width+1),  // contribution to the critical path of this VHDL operation
	//									"Ti1",                                // signal name (will be returned by declare())
	//									output_width+1)                           // signal size, in bits
	//									 << " <= (Xi" << of(msbin-lsbin) << " & Xi" << range(input_width-1,g-1) << ") + (Yi" << of(msbin-lsbin) << " & Yi"  << 	//									range(input_width-1,g-1) << ");" << endl;
	//									
	//
	//					vhdl <<tab<< declare(
	//									getTarget()->adderDelay(output_width+1),  // contribution to the critical path of this VHDL operation
	//									"Ti2",                                // signal name (will be returned by declare())
	//									output_width+1)                           // signal size, in bits
	//									 << " <= Ti1 + (" << zg(output_width)  << " & \"1\" );" << endl;
	//
	//					vhdl << tab << "Si" <<  range(output_width-1,0) << "<= Ti2" << range(output_width,1) << ";" << endl;
					}

				}
				//addComment("then place the last param1 bits"); 

			}
			else  //Subtraction
			{

				if(!laststage)
				{
					vhdl <<tab<< declare(
													getTarget()->adderDelay(input_width+1),  // contribution to the critical path of this VHDL operation
													"Tr",                                // signal name (will be returned by declare())
													input_width+1)                           // signal size, in bits
							 << " <= (Xr" << of(msbin-lsbin) << " & Xr) - (Yr" << of(msbin-lsbin) << " & Yr);" << endl;
					vhdl << tab << "Sr" <<  range(input_width+lsb_diff,lsb_diff) << " <= Tr" << range(input_width,0) << ";" << endl;
					
					// Imaginary part
					vhdl <<tab<< declare(
													getTarget()->adderDelay(input_width+1),  // contribution to the critical path of this VHDL operation
													"Ti",                                // signal name (will be returned by declare() )
													input_width+1)                           // signal size, in bits
							 << " <= (Xi" << of(msbin-lsbin) << " & Xi) - (Yi" << of(msbin-lsbin) << " & Yi);" << endl;
					vhdl << tab << "Si" <<  range(input_width+lsb_diff,lsb_diff) << "<= Ti" << range(input_width,0) << ";" << endl;

				}
				else // Last stage of FFT
				{
					if (g == 0)  // The topmost butterfly, without multiplication, without guardbits
					{
						vhdl <<tab<< declare(
										getTarget()->adderDelay(output_width),  // contribution to the critical path of this VHDL operation
										"Tr",                                // signal name (will be returned by declare())
										output_width)                           // signal size, in bits
								 << " <= (Xr" << of(msbin-lsbin) << " & Xr) - (Yr" << of(msbin-lsbin) << " & Yr);" << endl;
						vhdl << tab << "Sr <= Tr;" << endl;
						
						vhdl <<tab<< declare(
										getTarget()->adderDelay(output_width),  // contribution to the critical path of this VHDL operation
										"Ti",                                // signal name (will be returned by declare())
										output_width)                           // signal size, in bits
							 << " <= (Xi" << of(msbin-lsbin) << " & Xi) - (Yi" << of(msbin-lsbin) << " & Yi);" << endl;
						vhdl << tab << "Si <= Ti;" << endl;
					}
					else
					{

						vhdl <<tab<< declare(
										getTarget()->adderDelay(output_width+1),  // contribution to the critical path of this VHDL operation
										"Tr",                                // signal name (will be returned by declare())
										output_width+1)                           // signal size, in bits
										 << " <= (Xr" << of(msbin-lsbin) << " & Xr" << range(input_width-1,g-1) << ") - (Yr" << of(msbin-lsbin) << " & Yr"  << 
											range(input_width-1,g-1) << ") + (" << zg(output_width)  << " & \"1\" );" << endl;
											//range(input_width-1,g-1) << ");" << endl;
						vhdl << tab << "Sr" <<  range(output_width-1,0) << "<= Tr" << range(output_width,1) << ";" << endl;

					/*	vhdl <<tab<< declare(
										getTarget()->adderDelay(output_width+1),  // contribution to the critical path of this VHDL operation
										"Tr1",                                // signal name (will be returned by declare())
										output_width+1)                           // signal size, in bits
										 << " <= (Xr" << of(msbin-lsbin) << " & Xr" << range(input_width-1,g-1) << ") - (Yr" << of(msbin-lsbin) << " & Yr"  << 										range(input_width-1,g-1) << ");" << endl;
										

						vhdl <<tab<< declare(
										getTarget()->adderDelay(output_width+1),  // contribution to the critical path of this VHDL operation
										"Tr2",                                // signal name (will be returned by declare())
										output_width+1)                           // signal size, in bits
										 << " <= Tr1 + (" << zg(output_width)  << " & \"1\" );" << endl;

						vhdl << tab << "Sr" <<  range(output_width-1,0) << "<= Tr2" << range(output_width,1) << ";" << endl;
	*/

						// Imaginary part
						vhdl <<tab<< declare(
										getTarget()->adderDelay(output_width+1),  // contribution to the critical path of this VHDL operation
										"Ti",                                // signal name (will be returned by declare())
										output_width+1)                           // signal size, in bits
										 << " <= (Xi" << of(msbin-lsbin) << " & Xi" << range(input_width-1,g-1) << ") - (Yi" << of(msbin-lsbin) << " & Yi"  << 
											range(input_width-1,g-1) << ") + (" << zg(output_width)  << " & \"1\" );" << endl;
											//range(input_width-1,g-1) << ");" << endl;
						vhdl << tab << "Si" <<  range(output_width-1,0) << "<= Ti" << range(output_width,1) << ";" << endl;


					/*	vhdl <<tab<< declare(
										getTarget()->adderDelay(output_width+1),  // contribution to the critical path of this VHDL operation
										"Ti1",                                // signal name (will be returned by declare())
										output_width+1)                           // signal size, in bits
										 << " <= (Xi" << of(msbin-lsbin) << " & Xi" << range(input_width-1,g-1) << ") - (Yi" << of(msbin-lsbin) << " & Yi"  << 										range(input_width-1,g-1) << ");" << endl;
										

						vhdl <<tab<< declare(
										getTarget()->adderDelay(output_width+1),  // contribution to the critical path of this VHDL operation
										"Ti2",                                // signal name (will be returned by declare())
										output_width+1)                           // signal size, in bits
										 << " <= Ti1 + (" << zg(output_width)  << " & \"1\" );" << endl;

						vhdl << tab << "Si" <<  range(output_width-1,0) << "<= Ti2" << range(output_width,1) << ";" << endl;
					*/
					
					}
				}

			}

			if(msbout > msbin+1)
			{
				vhdl << tab << "Sr" <<  of(output_width-1) << "<= Tr" << of(input_width) << ";" << endl;				
				vhdl << tab << "Si" <<  of(output_width-1) << "<= Ti" << of(input_width) << ";" << endl;				
			}
		}
		else
		{
			if (addorsub)
			{
				vhdl <<tab<< declare(
												//getTarget()->adderDelay(output_width),  // contribution to the critical path of this VHDL operation
												"Tr",                                // signal name (will be returned by declare())
												output_width)                           // signal size, in bits
						 << " <= (0 & Xr) + (0 & Yr);" << endl;

				vhdl << tab << "Sr <= Tr" << range(output_width - 1,0) << ";" << endl;
				//addComment("then place the last param1 bits"); 

				vhdl <<tab<< declare(
												//getTarget()->adderDelay(output_width),  // contribution to the critical path of this VHDL operation
												"Ti",                                // signal name (will be returned by declare() )
												output_width)                           // signal size, in bits
						 << " <= (0 & Xi) + (0 & Yi);" << endl;

				//addComment("first put the most significant bit of the result into R"); // addComment for small left-aligned comment
				vhdl << tab << "Si <= Ti" << range(output_width - 1,0) << ";" << endl;
			}
			else
			{
				vhdl <<tab<< declare(
												//getTarget()->adderDelay(output_width),  // contribution to the critical path of this VHDL operation
												"Tr",                                // signal name (will be returned by declare())
												output_width)                           // signal size, in bits
						 << " <= (0 & Xr) - (0 & Yr);" << endl;

				vhdl << tab << "Sr <= Tr" << range(output_width - 1,0) << ";" << endl;

				vhdl <<tab<< declare(
												//getTarget()->adderDelay(output_width),  // contribution to the critical path of this VHDL operation
												"Ti",                                // signal name (will be returned by declare() )
												output_width)                           // signal size, in bits
						 << " <= (0 & Xi) - (0 & Yi);" << endl;
				vhdl << tab << "Si <= Ti" << range(output_width - 1,0) << ";" << endl;

			}

		}

		addFullComment("End of vhdl generation"); // this will be a large, centered comment in the VHDL
	};

	
	void FixComplexAdder::emulate(TestCase * tc) {
		/* This function will be used when the TestBench command is used in the command line
		   we have to provide a complete and correct emulation of the operator, in order to compare correct output generated by this function with the test input generated by the vhdl code */
		/* first we are going to format the entries */
//		mpz_class sx = tc->getInputValue("X");
//		mpz_class sy = tc->getInputValue("Y");
//		mpz_class sz = tc->getInputValue("Z");

		/* then we are going to manipulate our bit vectors in order to get the correct output*/
//		mpz_class sr;
//		mpz_class stmp;
//		stmp = sx + sy + sz;
//		sr = (stmp % mpzpow2(param1)); // we delete all the bits that do not fit in the range (param1 - 1 downto 0);
//		sr += (stmp / mpzpow2 (param0+1)); // we add the first bit

		/* at the end, we indicate to the TestCase object what is the expected
		   output corresponding to the inputs */
//		tc->addExpectedOutput("R",sr);
	}


//	void FixComplexAdder::buildStandardTestCases(TestCaseList * tcl) {
		// please fill me with regression tests or corner case tests!
//	}




	OperatorPtr FixComplexAdder::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int msbin;
		UserInterface::parseInt(args, "msbin", &msbin);
		int lsbin;
		UserInterface::parseInt(args, "lsbin", &lsbin);
		int msbout;
		UserInterface::parseInt(args, "msbout", &msbout);
		int lsbout;
		UserInterface::parseInt(args, "lsbout", &lsbout);
		bool signedIn;
		UserInterface::parseBoolean(args, "signedIn", &signedIn);
		bool addorsub;
		UserInterface::parseBoolean(args, "addorsub", &addorsub);
		bool laststage;
		UserInterface::parseBoolean(args, "laststage", &laststage);

		return new FixComplexAdder(parentOp, target, msbin, lsbin, msbout, lsbout, signedIn, addorsub, laststage);
	}
	
	void FixComplexAdder::registerFactory(){
		UserInterface::add("FixComplexAdder", // name
											 "A Complex adder subtractor.",
											 "Complex", // categories
											 "",
											 "msbin(int): integer input most significant bit;\
							lsbin(int): integer input least significant bit;\
							msbout(int): integer output most significant bit;\
							lsbout(int): integer output least significant bit;\
							signedIn(bool)=true: if false calculation will be unsigned ;\
							addorsub(bool)=true: if false subtraction on the inputs ;\
							laststage(bool)=false: if true: last stage of FFT",
						 	"See the developper manual in the doc/ directory of FloPoCo.",
			 				FixComplexAdder::parseArguments
							) ;

	}

}//namespace
