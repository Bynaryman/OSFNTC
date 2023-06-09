/*
  A test bench generator for FloPoCo.

  Authors: Cristian Klein, Florent de Dinechin, Nicolas Brunie

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2017.
  All rights reserved.

 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "utils.hpp"
#include "Operator.hpp"
#include "TestBench.hpp"

using namespace std;

namespace flopoco{


	TestBench::TestBench(Target* target, Operator* op, int n, bool fromFile):
		Operator(nullptr, target), op_(op), n_(n), fromFile_(fromFile)
	{
		//We do not set the parent operator to this operator
		setNoParseNoSchedule();

		useNumericStd();
		// This allows the op under test to know how long it is being tested.
		// useful only for testing the long acc, but who knows.
		op->numberOfTests = n;

		srcFileName="TestBench";
		setNameWithFreqAndUID("TestBench_" + op_->getName());

		//		REPORT(LIST,"Test bench for "+ op_->getName());

		// initialization of flopoco random generator
		// TODO : has to be initialized before any use of getLargeRandom or getRandomIEEE...
		//        maybe best to be placed in main.cpp ?
		FloPoCoRandomState::init(n);
		// Generate the standard and random test cases for this operator
		op-> buildStandardTestCases(&tcl_);
		// initialization of randomstate generator with the seed base on the number of
		// random testcase to be generated
		if (!fromFile) op-> buildRandomTestCaseList(&tcl_, n);


		// The instance
		//  portmap for the inputs and outputs
		for(int i=0; i < op->getIOListSize(); i++){
			Signal* s = op->getIOListSignal(i);

			if(s->type() == Signal::in) {
				declare(s->getName(), s->width(), s->isBus());
				inPortMap (s->getName(), s->getName());
			}
			if(s->type() == Signal::out)
				outPortMap (s->getName(), s->getName());
		}


		// add clk and rst
		declare("clk");
		declare("rst");
		// declaration and assignation of the clock enable signal
		if (op_->hasClockEnable()) {
			vhdl << tab << declare("ce") << " <= '1';" << endl;
		}

		// The VHDL for the instance
		vhdl << endl << instance(op, "test", false) << endl;
		subComponentList_.clear(); // it is unfortunately set by instance()

		vhdl << tab << "-- Ticking clock signal" <<endl;
		vhdl << tab << "process" <<endl;
		vhdl << tab << "begin" <<endl;
		vhdl << tab << tab << "clk <= '1';" <<endl;
		vhdl << tab << tab << "wait for 5 ns;" <<endl;
		vhdl << tab << tab << "clk <= '0';" <<endl;
		vhdl << tab << tab << "wait for 5 ns;" <<endl;
		vhdl << tab << "end process;" <<endl;
		vhdl << endl;

		setSequential();

		if (fromFile)
			generateTestFromFile();
		else
			generateTestInVhdl();
	}








	/* Generating the tests using a file to store the IO, allow to have a lot of IOs without
	 * increasing the VHDL compilation time
	 */
	void TestBench::generateTestFromFile() {
		vector<Signal*> inputSignalVector;
		vector<Signal*> outputSignalVector;

		for(int i=0; i < op_->getIOListSize(); i++){
			Signal* s = op_->getIOListSignal(i);
			if (s->type() == Signal::out)
				outputSignalVector.push_back(s);
			else if (s->type() == Signal::in)
				inputSignalVector.push_back(s);
		};

		// declaration of test time
		int currentOutputTime = 0;

		// In order to generate the file containing inputs and expected output in a correct order
		// we will store the use order for file decompression
		list<string> IOorderInput;
		list<string> IOorderOutput;


		vhdl << tab << "-- Reading the input from a file " << endl;
		vhdl << tab << "process" <<endl;


		/* Variable declaration */
		vhdl << tab << tab << "variable inline : line; " << endl;                    // variable to read a line
		vhdl << tab << tab << "variable counter : integer := 1;" << endl;
		vhdl << tab << tab << "variable errorCounter : integer := 0;" << endl;
		vhdl << tab << tab << "variable possibilityNumber : integer := 0;" << endl;
		vhdl << tab << tab << "variable localErrorCounter : integer := 0;" << endl;
		vhdl << tab << tab << "variable tmpChar : character;" << endl;                        // variable to store a character (escape between inputs)
		vhdl << tab << tab << "file inputsFile : text is \"test.input\"; " << endl; // declaration of the input file


		/* Variable to store value for inputs and expected outputs*/
		for(int i=0; i < op_->getIOListSize(); i++){
			Signal* s = op_->getIOListSignal(i);
			vhdl << tab << tab << "variable V_" << s->getName();
			/*if (s->width() != 1)*/ vhdl << " : bit_vector("<< s->width() - 1 << " downto 0);" << endl;
			//else vhdl << " : bit;" << endl;
		}

		/* Process Beginning */
		vhdl << tab << "begin" << endl;

		/* Reset Sending */
		vhdl << tab << tab << "-- Send reset" <<endl;
		vhdl << tab << tab << "rst <= '1';" << endl;
		vhdl << tab << tab << "wait for 15 ns;" << endl;  // we want reset bigger than 1 tick
		vhdl << tab << tab << "rst <= '0';" << endl;
		vhdl << tab << tab << "wait for 15 ns;" << endl;  // align inputs

		/* File Reading */
		vhdl << tab << tab << "while not endfile(inputsFile) loop" << endl;
		vhdl << tab << tab << tab << " -- positionning inputs" << endl;

		/* All inputs and the corresponding expected outputs will be on the same line
		 * so we begin by reading this line, once and for all (once by test) */
		vhdl << tab << tab << tab << "readline(inputsFile,inline);" << endl;

		// input reading and forwarding to the operator
		for(unsigned int i=0; i < inputSignalVector.size(); i++){
			Signal* s = inputSignalVector[i];
			vhdl << tab << tab << tab << "read(inline ,V_"<< s->getName() << ");" << endl;
			vhdl << tab << tab << tab << "read(inline,tmpChar);" << endl; // we consume the character between each inputs
			if ((s->width() == 1) && (!s->isBus())) vhdl << tab << tab << tab << s->getName() << " <= to_stdlogicvector(V_" << s->getName() << ")(0);" << endl;
			else vhdl << tab << tab << tab << s->getName() << " <= to_stdlogicvector(V_" << s->getName() << ");" << endl;
			// adding the IO to IOorder
			IOorderInput.push_back(s->getName());
		}
		vhdl << tab << tab << tab << "readline(inputsFile,inline);" << endl;  // it consume output line
		vhdl << tab << tab << tab << "wait for 10 ns;" << endl; // let 10 ns between each input
		vhdl << tab << tab << "end loop;" << endl;
		vhdl << tab << tab << "wait for 10000 ns; -- wait for simulation to finish" << endl; // TODO : tune correctly with pipeline depth
		vhdl << tab << "end process;" << endl;

		/**
		 * Declaration of a waiting time between sending the input and comparing
		 * the result with the output
		 * in case of a pipelined operator we have to wait the complete latency of all the operator
		 * that means all the pipeline stages each step
		 * TODO : entrelaced the inputs / outputs in order to avoid this wait
		 */
		vhdl << tab << tab << tab << " -- verifying the corresponding output" << endl;
		vhdl << tab << tab << tab << "process" << endl;
		/* Variable declaration */
		vhdl << tab << tab << "variable inline0 : line; " << endl;                    // variable to read a line
		vhdl << tab << tab << "variable inline : line; " << endl;                    // variable to read a line
		vhdl << tab << tab << "variable counter : integer := 1;" << endl;
		vhdl << tab << tab << "variable errorCounter : integer := 0;" << endl;
		vhdl << tab << tab << "variable possibilityNumber : integer := 0;" << endl;
		vhdl << tab << tab << "variable localErrorCounter : integer := 0;" << endl;
		vhdl << tab << tab << "variable tmpChar : character;" << endl;                        // variable to store a character (escape between inputs)
		//vhdl << tab << tab << "variable tmpString : string;" << endl;
		vhdl << tab << tab << "file inputsFile : text is \"test.input\"; " << endl; // declaration of the input file


		/* Variable to store value for inputs and expected outputs*/
		for(Signal* s: outputSignalVector){
			vhdl << tab << tab << "variable V_" << s->getName();
			if ((s->width() != 1) || (s->isBus())) vhdl << " : bit_vector("<< s->width() - 1 << " downto 0);" << endl;
			else  vhdl << " : bit;" << endl;
			vhdl << tab << tab << "variable expected_"  << s->getName() << ": string (1 to 1000);" << endl; // will be a copy of inline
			vhdl << tab << tab << "variable expected_size_"  << s->getName() << " : integer;" << endl;
		}

		/* Process Beginning */
		vhdl << tab << "begin" << endl;

		vhdl << tab << tab << tab << " wait for 20 ns;" << endl; // wait for reset signal to finish
		currentOutputTime += 20;
		if (op_->getPipelineDepth() > 0){
			vhdl << tab << tab << "wait for "<< op_->getPipelineDepth()*10 <<" ns; -- wait for pipeline to flush" <<endl;
			currentOutputTime += op_->getPipelineDepth()*10;
		} else {
			vhdl << tab << tab << "wait for "<< 2 <<" ns; -- no pipeline here" <<endl;
			currentOutputTime += 2;
		};


		/* File Reading */
		vhdl << tab << tab << "while not endfile(inputsFile) loop" << endl;
		vhdl << tab << tab << tab << " -- positionning inputs" << endl;

		/* All inputs and the corresponding expected outputs will be on the same line
		 * so we begin by reading this line, once and for all (once by test) */
		vhdl << tab << tab << tab << "readline(inputsFile,inline0);" << endl; // it consumes input line
		vhdl << tab << tab << tab << "readline(inputsFile,inline);" << endl;

		// vhdl << tab << tab << tab << "wait for "<< op_->getPipelineDepth()*10 <<" ns; -- wait for pipeline to flush" <<endl;
		for(Signal* s: outputSignalVector){
			vhdl << tab << tab << tab << "read(inline, possibilityNumber);" << endl;
			vhdl << tab << tab << tab << "localErrorCounter := 0;" << endl;
			vhdl << tab << tab << tab << "read(inline,tmpChar);" << endl; // we consume the character after output list
			vhdl << tab << tab << tab << "expected_size_"<< s->getName() << " := inline'Length;"<< endl; // the remainder is the vector of expected outputs: remember how long it is
			vhdl << tab << tab << tab << "expected_"<< s->getName() << " := inline.all & (expected_size_"<< s->getName() << "+1 to 1000 => ' ');"<< endl; // because we have to pad it to 1000 chars
			string expectedString = "expected_" +  s->getName() + "(1 to expected_size_" + s->getName() + ")"; //  will be used several times below, so better have a Single Source of Bug
			vhdl << tab << tab << tab << "if possibilityNumber = 0 then" << endl;
			vhdl << tab << tab << tab << tab << "localErrorCounter := 0;" << endl;//read(inline,tmpChar);" << endl; // we consume the character between each outputs
			vhdl << tab << tab << tab << "elsif possibilityNumber = 1 then " << endl;
			vhdl << tab << tab << tab << tab << "read(inline ,V_"<< s->getName() << ");" << endl;
			vhdl << tab << tab << tab << tab << "if ";
			if (s->isFP()) {
				vhdl << "not fp_equal(fp"<< s->width() << "'(" << s->getName() << ") ,to_stdlogicvector(V_" <<  s->getName() << "))";
			} else if (s->isIEEE()) {
			    vhdl << "not fp_equal_ieee(" << s->getName() << " ,to_stdlogicvector(V_" <<  s->getName() << "),"<<s->wE()<<" , "<<s->wF()<<")";
			} else if ((s->width() == 1) && (!s->isBus())) {
				vhdl << "not (" << s->getName() << "= to_stdlogic(V_" << s->getName() << "))";
			} else {
				vhdl << "not (" << s->getName() << "= to_stdlogicvector(V_" << s->getName() << "))";
			}
			vhdl << " then " << endl;
			vhdl << tab << tab << tab << tab << tab << " errorCounter := errorCounter + 1;" << endl;
			vhdl << tab << tab << tab << tab << tab << "assert false report(\"Line \" & integer'image(counter) & \" of input file, incorrect output for "
					 << s->getName() << ": \" & lf & ";
			vhdl << "\"  expected value: \" & "  << expectedString;
			vhdl << " & lf & \"          result: \" & str(" << s->getName() <<")) ;"<< endl;
			vhdl << tab << tab << tab << tab << "end if;" << endl;

			vhdl << tab << tab << tab << "else" << endl;
			vhdl << tab << tab << tab << tab << "for i in possibilityNumber downto 1 loop " << endl;
			vhdl << tab << tab << tab << tab << tab << "read(inline ,V_"<< s->getName() << ");" << endl;
			vhdl << tab << tab << tab << tab << tab << "read(inline,tmpChar);" << endl; // we consume the character between each outputs
			if (s->isFP()) {
				vhdl << tab << tab << tab << tab << tab << "if fp_equal(fp"<< s->width() << "'(" << s->getName() << ") ,to_stdlogicvector(V_" <<  s->getName() << ")) " << "  then localErrorCounter := 1; end if; " << endl;
			} else if (s->isIEEE()) {
				vhdl << tab << tab << tab << tab << tab << "if fp_equal_ieee(" << s->getName() << " ,to_stdlogicvector(V_" <<  s->getName() << "),"<<s->wE()<<" , "<<s->wF()<<")" << " then localErrorCounter := 1; end if;" << endl;
			} else if ((s->width() == 1) && (!s->isBus())) {
				vhdl << tab << tab << tab << tab << tab << "if (" << s->getName() << "= to_stdlogic(V_" << s->getName() << ")) " << " then localErrorCounter := 1; end if;" << endl;
			} else {
				vhdl << tab << tab << tab << tab << tab << "if (" << s->getName() << "= to_stdlogicvector(V_" << s->getName() << ")) " << " then localErrorCounter := 1; end if;" << endl;
			}
			vhdl << tab << tab << tab << tab << "end loop;" << endl;
			vhdl << tab << tab << tab << tab << " if (localErrorCounter = 0) then " << endl;
			vhdl << tab << tab << tab << tab << tab << "errorCounter := errorCounter + 1; -- incrementing global error counter" << endl;

			// **** better aligned reporting here ****
			vhdl << tab << tab << tab << tab << tab << "assert false report(\"Line \" & integer'image(counter) & \" of input file, incorrect output for "
					 << s->getName() << ": \" & lf & ";
			vhdl << "\" expected values: \" & "  << expectedString;
			vhdl << " & lf & \"          result: \" & str(" << s->getName() <<")) ;"<< endl;

			vhdl << tab << tab << tab << tab << "end if;" << endl;
			vhdl << tab << tab << tab << "end if;" << endl;
			// TODO add test to increment global error counter
			/* adding the IO to the IOorder list */
			IOorderOutput.push_back(s->getName());
		};
		vhdl << tab << tab << tab << " wait for 10 ns; -- wait for pipeline to flush" << endl;
		currentOutputTime += 10 * (tcl_.getNumberOfTestCases()+n_); // time for simulation
		vhdl << tab << tab << tab << "counter := counter + 2;" << endl; // incrementing by 2 because a testcase takes two lines (one for input, one for output)
		vhdl << tab << tab << "end loop;" << endl;
		vhdl << tab << tab << "report (integer'image(errorCounter) & \" error(s) encoutered.\");" << endl;
		vhdl << tab << tab << "report \"End of simulation\" severity note;" <<endl;
		vhdl << tab << "end process;" <<endl;

		/* Setting the computed simulation Time */
		simulationTime = currentOutputTime;

		/* Generating a file of inputs */
		// opening a file to write down the output (for text-file based test)
		// if n < 0 we do not generate a file
		if (n_ >= 0) {
			string inputFileName = "test.input";
			ofstream fileOut(inputFileName.c_str(),ios::out);
			// if error at opening, let's mention it !
			if (!fileOut) cerr << "FloPoCo was not abe to open " << inputFileName << " in order to write down inputs. " << endl;
			for (int i = 0; i < tcl_.getNumberOfTestCases(); i++)	{
				TestCase* tc = tcl_.getTestCase(i);
				if (fileOut) fileOut << tc->generateInputString(IOorderInput,IOorderOutput);
			}

			//let's not reinvent the wheel
			/*
			// generation on the fly of random test case
			for (int i = 0; i < n_; i++) {
				TestCase* tc = op_->buildRandomTestCase(i);
				if (fileOut) fileOut << tc->generateInputString(IOorderInput,IOorderOutput);
				delete tc;
			};
			*/
			TestCaseList *tcl = new TestCaseList();

			op_->buildRandomTestCaseList(tcl, n_);
			for (int i = 0; i < n_; i++) {
				TestCase* tc = tcl->getTestCase(i);
				if (fileOut) fileOut << tc->generateInputString(IOorderInput,IOorderOutput);
				delete tc;
			};
			delete tcl;

			// closing input file
			fileOut.close();
		};


		// exhaustive test IO generation
		if(n_ == -2) {
			string inputFileName = "test.input";
			ofstream fileOut(inputFileName.c_str(),ios::out);
			// if error at opening, let's mention it !
			if (!fileOut)
				THROWERROR("Not able to open " << inputFileName << " in order to write inputs. ");

			REPORT(LIST,"Generating the exhaustive test bench, this may take some time");
			// exhaustive test
			int length = inputSignalVector.size();
			//			REPORT(0, "Found " << length << "inputs");
			int* InWidth = new int[length];
			// getting signal width
			for (int i = 0; i < length; i++) InWidth[i] = inputSignalVector[i]->width();
			mpz_class* bound = new mpz_class[length];
			mpz_class* counters = new mpz_class[length];
			int number = 1;
			for (int i = 0; i < length; i++) {
				bound[i] = (mpz_class(1) << InWidth[i]);// * tmp;
				counters[i] = 0;
				number *= intpow2(InWidth[i]);
			}
			// getting signal name
			string* IOname = new string[length];
			for (int i = 0; i < length; i++) IOname[i] = inputSignalVector[i]->getName();
			TestCase* tc;

			// simulation time computation
			currentOutputTime = 0;
			// init
			currentOutputTime += 10;
			currentOutputTime += 5 * number;
			currentOutputTime += op_->getPipelineDepth()*10;
			currentOutputTime += 5 * number;
			currentOutputTime += 2;
			simulationTime=currentOutputTime;


			while (true) {
				for (int i = 0; i < length-1; i++) {
            		if (counters[i] >= bound[i]) {
						counters[i] = 0;
						counters[i+1] += 1;
					}
				}
				// if the max counter overflows, break
				if (counters[length-1] >= bound[length-1]) break;
				// Test Case inputs
				tc = new TestCase(op_);
				for (int i = 0; i < length; i++) {
					tc->addInput(IOname[i],counters[i]);
				}
				op_->emulate(tc);
				if (fileOut) fileOut << tc->generateInputString(IOorderInput,IOorderOutput);
				// incrementation
				counters[0]++;
				delete tc;
			}
			fileOut.close();
		}
	}


	void TestBench::generateTestInVhdl() {
		vhdl << tab << "-- Setting the inputs" <<endl;
		vhdl << tab << "process" <<endl;
		vhdl << tab << "begin" <<endl;
		vhdl << tab << tab << "-- Send reset" <<endl;
		vhdl << tab << tab << "rst <= '1';" << endl;
		vhdl << tab << tab << "wait for 10 ns;" << endl;
		vhdl << tab << tab << "rst <= '0';" << endl;
		for (int i = 0; i < tcl_.getNumberOfTestCases(); i++){
			vhdl << tcl_.getTestCase(i)->getInputVHDL(tab + tab);
			vhdl << tab << tab << "wait for 10 ns;" <<endl;
		}
		/* COULD NOT BE USED BECAUSE IT HAS TO BE THE SAME TESTCASE FOR INPUT AND OUTPUT GENERATION
		// generation on the fly of random test case (VALID only for FPFMA)
		for (int i = 0; i < n_; i++) {
		TestCase* tc = op_->buildRandomTestCases(i);
		vhdl << tc->getInputVHDL(tab + tab);
		vhdl << tab << tab << "wait for 10ns;" << endl;
		delete tc;
		};*/
		vhdl << tab << tab << "wait for 100000 ns; -- allow simulation to finish" << endl;
		vhdl << tab << "end process;" <<endl;
		vhdl <<endl;

		int currentOutputTime = 0;
		vhdl << tab << "-- Checking the outputs" <<endl;
		vhdl << tab << "process" <<endl;
		vhdl << tab << "begin" <<endl;
		vhdl << tab << tab << "wait for 10 ns; -- wait for reset to complete" <<endl;
		currentOutputTime += 10;
		if (op_->getPipelineDepth() > 0){
			vhdl << tab << tab << "wait for "<< op_->getPipelineDepth()*10 <<" ns; -- wait for pipeline to flush" <<endl;
			currentOutputTime += op_->getPipelineDepth()*10;
		}
		else{
			vhdl << tab << tab << "wait for "<< 2 <<" ns; -- no pipeline here" <<endl;
			currentOutputTime += 2;
		}
		for (int i = 0; i < tcl_.getNumberOfTestCases(); i++) {
			vhdl << tab << tab << "-- current time: " << currentOutputTime <<endl;
			TestCase* tc = tcl_.getTestCase(i);
			if (tc->getComment() != "")
				vhdl << tab <<  "-- " << tc->getComment() << endl;
			vhdl << tc->getInputVHDL(tab + tab + "-- input: ");
			vhdl << tc->getExpectedOutputVHDL(tab + tab);
			vhdl << tab << tab << "wait for 10 ns;" <<endl;
			currentOutputTime += 10;
		}
		/* SEE REMARK FOR ON THE FLY INPUT GENERATION
                // generation on the fly of random test case (VALID only for FPFMA)
                for (int i = 0; i < n_; i++) {
                        TestCase* tc = op_->buildRandomTestCases(i);
			if (tc->getComment() != "")
				vhdl << tab <<  "-- " << tc->getComment() << endl;
			vhdl << tc->getInputVHDL(tab + tab + "-- input: ");
			vhdl << tc->getExpectedOutputVHDL(tab + tab);
			vhdl << tab << tab << "wait for 10 ns;" <<endl;
			currentOutputTime += 10;
                        delete tc;
                };*/

		vhdl << tab << tab << "assert false report \"End of simulation\" severity failure;" <<endl;
		vhdl << tab << "end process;" <<endl;

		simulationTime=currentOutputTime;
	}

	TestBench::~TestBench() {
	}


	void TestBench::outputVHDL(ostream& o, string name) {
		licence(o,"Florent de Dinechin, Cristian Klein, Nicolas Brunie (2007-2010)");

		//was part of constructor, but needs to be done here, after the operators have been scheduled
		/*
		if (fromFile_)
			generateTestFromFile();
		else
			generateTestInVhdl();
		*/

		Operator::stdLibs(o);

		outputVHDLEntity(o);
		o << "architecture behavorial of " << name  << " is" << endl;

		// the operator to wrap
		op_->outputVHDLComponent(o);
		// The local signals
		o << buildVHDLSignalDeclarations();

		o << endl <<
			tab << "-- FP compare function (found vs. real)\n" <<
			tab << "function fp_equal(a : std_logic_vector; b : std_logic_vector) return boolean is\n" <<
			tab << "begin\n" <<
			tab << tab << "if b(b'high downto b'high-1) = \"01\" then\n" <<
			tab << tab << tab << "return a = b;\n" <<
			tab << tab << "elsif b(b'high downto b'high-1) = \"11\" then\n" <<
			tab << tab << tab << "return (a(a'high downto a'high-1)=b(b'high downto b'high-1));\n" <<
			tab << tab << "else\n" <<
			tab << tab << tab << "return a(a'high downto a'high-2) = b(b'high downto b'high-2);\n" <<
			tab << tab << "end if;\n" <<
			tab << "end;\n";


		o << endl << endl << endl;
		/* Generation of Vhdl function to parse file into std_logic_vector */


		o << " -- converts std_logic into a character" << endl;
		o << tab << "function chr(sl: std_logic) return character is" << endl
		  << tab << tab << "variable c: character;" << endl
		  << tab << "begin" << endl
		  << tab << tab << "case sl is" << endl
		  << tab << tab << tab << "when 'U' => c:= 'U';" << endl
		  << tab << tab << tab << "when 'X' => c:= 'X';" << endl
		  << tab << tab << tab << "when '0' => c:= '0';" << endl
		  << tab << tab << tab << "when '1' => c:= '1';" << endl
		  << tab << tab << tab << "when 'Z' => c:= 'Z';" << endl
		  << tab << tab << tab << "when 'W' => c:= 'W';" << endl
		  << tab << tab << tab << "when 'L' => c:= 'L';" << endl
		  << tab << tab << tab << "when 'H' => c:= 'H';" << endl
		  << tab << tab << tab << "when '-' => c:= '-';" << endl
		  << tab << tab << "end case;" << endl
		  << tab << tab << "return c;" << endl
		  << tab <<  "end chr;" << endl;


		o << tab << "-- converts bit to std_logic (1 to 1)" << endl
			<<	tab << "function to_stdlogic(b : bit) return std_logic is" << endl
			<< tab << tab << " variable sl : std_logic;" << endl
			<< tab << "begin" << endl
			<< tab << tab << "case b is " << endl
			<< tab << tab << tab << "when '0' => sl := '0';" << endl
			<< tab << tab << tab << "when '1' => sl := '1';" << endl
			<< tab << tab << "end case;" << endl
			<< tab << tab << "return sl;" << endl
			<< tab << "end to_stdlogic;" << endl;


		o << tab << "-- converts std_logic into a string (1 to 1)" << endl
		  << tab << "function str(sl: std_logic) return string is" << endl
		  << tab << " variable s: string(1 to 1);" << endl
		  << tab << " begin" << endl
		  << tab << tab << "s(1) := chr(sl);" << endl
		  << tab << tab << "return s;" << endl
		  << tab << "end str;" << endl;



		o << tab << "-- converts std_logic_vector into a string (binary base)" << endl
		  << tab << "-- (this also takes care of the fact that the range of" << endl
		  << tab << "--  a string is natural while a std_logic_vector may" << endl
		  << tab << "--  have an integer range)" << endl
		  << tab << "function str(slv: std_logic_vector) return string is" << endl
		  << tab << tab << "variable result : string (1 to slv'length);" << endl
		  << tab << tab << "variable r : integer;" << endl
		  << tab << "begin" << endl
		  << tab << tab << "r := 1;" << endl
		  << tab << tab << "for i in slv'range loop" << endl
		  << tab << tab << tab << "result(r) := chr(slv(i));" << endl
		  << tab << tab << tab << "r := r + 1;" << endl
		  << tab << tab << "end loop;" << endl
		  << tab << tab << "return result;" << endl
		  << tab << "end str;" << endl;

		o << endl << endl << endl;


		/* If op_ is an IEEE operator (IEEE input and output, we define) the function
		 * fp_equal for the considered precision in the ieee case
		 */
		o << endl <<  // Fixed by Nicolas
			tab << "-- test isZero\n" <<
			tab << "function iszero(a : std_logic_vector) return boolean is\n" <<
			tab << "begin\n" <<
			tab << tab << "return  a = (a'high downto 0 => '0');\n" <<        // test if exponent = "0000---000"
			tab << "end;\n\n\n" <<

			tab << "-- FP IEEE compare function (found vs. real)\n" <<
			tab << "function fp_equal_ieee(a : std_logic_vector;"
                                                        << " b : std_logic_vector;"
                                                        << " we : integer;"
                                                        << " wf : integer) return boolean is\n"
		     <<	tab << "begin\n" <<
			tab << tab << "if a(wf+we downto wf) = b(wf+we downto wf) and b(we+wf-1 downto wf) = (we downto 1 => '1') then\n" <<        // test if exponent = "1111---111"
			tab << tab << tab << "if iszero(b(wf-1 downto 0)) then return  iszero(a(wf-1 downto 0));\n" <<               // +/- infinity cases
                        tab << tab << tab << "else return not iszero(a(wf - 1 downto 0));\n" <<
                        tab << tab << tab << "end if;\n" <<
			tab << tab << "else\n" <<
			tab << tab << tab << "return a(a'high downto 0) = b(b'high downto 0);\n" <<
			tab << tab << "end if;\n" <<
			tab << "end;\n";


		/* In VHDL, literals may be incorrectly converted to „std_logic_vector(... to ...)” instead
		 * of „downto”. So, for each FP output width, create a subtype used for casting.
		 */
		{
			std::set<int> widths;
			for (int i=0; i<op_->getIOListSize(); i++){
				Signal* s = op_->getIOListSignal(i);

				if (s->type() != Signal::out) continue;
				if (s->isFP() != true and s->isIEEE() != true) continue;
				//				std::cout << "debug : inserting size " << s->width() << " in fp subtype map. " << std::endl;
				widths.insert(s->width());
			}

			if (widths.size() > 0)
				o << endl << tab << "-- FP subtypes for casting\n";
			for (std::set<int>::iterator it = widths.begin(); it != widths.end(); it++)
				{
					int w = *it;
					o << tab << "subtype fp" << w << " is std_logic_vector(" << w-1 << " downto 0);\n";
				}
		}


		o << "begin\n";

		//if the outputs of the tested operator are not synchronized
		//	then registering and delaying the outputs might be necessary
		bool opHasOutputsDesync = false;

		for(unsigned i=0; i<op_->ioList_.size(); i++)
		{
			Signal *s = op_->getIOListSignal(i);

			if(s->type() != Signal::out)
				continue;

			for(unsigned j=0; j<op_->ioList_.size(); j++)
			{
				Signal *t = op_->getIOListSignal(j);

				if((t->type() == Signal::out) && (s->getName() != t->getName()) && (s->getCycle() != t->getCycle())){
					opHasOutputsDesync = true;
					break;
				}
			}

			if(opHasOutputsDesync)
				break;
		}
		if(opHasOutputsDesync == true)
			o << buildVHDLRegisters() << endl;

		//output the code of the
		o << vhdl.str() << endl;

		o << "end architecture;" << endl << endl;


	}


		/** Return the total simulation time*/
		int TestBench::getSimulationTime(){
			//			cerr << endl << endl<< simulationTime << endl << endl ;
			return simulationTime;
		}





	OperatorPtr TestBench::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int n;
		bool file;

		if(UserInterface::globalOpList.empty()){
			throw(string("TestBench has no operator to wrap (it should come after the operator it wraps)"));
		}

		UserInterface::parseInt(args, "n", &n);
		UserInterface::parseBoolean(args, "file", &file);
		Operator* toWrap = UserInterface::globalOpList.back();
		Operator* newOp = new TestBench(target, toWrap, n, file);
		// the instance in newOp has added toWrap as a subcomponent of newOp,
		// so we may remove it from globalOpList
		//UserInterface::globalOpList.pop_back();
		return newOp;
	}

	void TestBench::registerFactory(){
		UserInterface::add("TestBench", // name
											 "Behavorial test bench for the preceding operator.",
											 "TestBenches",
											 "fixed-point function evaluator; fixed-point", // categories
											 "n(int)=-2: number of random tests. If n=-2, an exhaustive test is generated (use only for small operators);\
                        file(bool)=true:Inputs and outputs are stored in file test.input (lower VHDL compilation time). If false, they are stored in the VHDL;",
											 "",
											 TestBench::parseArguments
											 ) ;

	}

}
