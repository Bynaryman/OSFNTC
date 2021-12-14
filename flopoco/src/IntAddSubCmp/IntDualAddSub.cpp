/*
  An operator which performes x-y and y-x in parallel for FloPoCo

  Author : Bogdan Pasca, Florent de Dinechin

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2010.
  All rights reserved.

*/

#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "utils.hpp"
#include "Operator.hpp"
#include "IntDualAddSub.hpp"
#include "IntAdder.hpp"

using namespace std;


namespace flopoco{

	IntDualAddSub::IntDualAddSub(Operator* parentOp, Target* target, int wIn, int opType):
		Operator(parentOp, target), wIn_(wIn), opType_(opType)
	{
		ostringstream name;
		srcFileName="IntDualAddSub";
		setCopyrightString("Bogdan Pasca, Florent de Dinechin (2008-2017)");

		if (opType==SUBSUB) {
			son = "YmX";
			name << "IntDualSub_";
		}
		else {
			son = "XpY";
			name << "IntDualAddSub_";
		}
		name << wIn;
		setNameWithFreqAndUID(name.str());

		// Set up the IO signals
		addInput ("X"  , wIn_, true);
		addInput ("Y"  , wIn_, true);
		addOutput("XmY", wIn_, 1, true);
		addOutput(son, wIn_, 1, true);



		// Most of this code is taken from IntAdder: please track the IntAdder code in the future.
		schedule();
		double targetPeriod = 1.0/getTarget()->frequency() - getTarget()->ffDelay();
		// What is the maximum lexicographic time of our inputs?
		int maxCycle = 0;
		double maxCP = 0.0;
		for(auto i: ioList_) {
			REPORT(DEBUG, "signal " << i->getName() <<  "  Cycle=" << i->getCycle() <<  "  criticalPath=" << i->getCriticalPath() );
			if((i->getCycle() > maxCycle)
					|| ((i->getCycle() == maxCycle) && (i->getCriticalPath() > maxCP)))	{
				maxCycle = i->getCycle();
				maxCP = i->getCriticalPath();
			}
		}
		double totalPeriod = maxCP + getTarget()->adderDelay(wIn);

		REPORT(DETAILED, "maxCycle=" << maxCycle <<  "  maxCP=" << maxCP <<  "  totalPeriod=" << totalPeriod <<  "  targetPeriod=" << targetPeriod );

		if(totalPeriod <= targetPeriod)		{
			vhdl << tab << declare(getTarget()->adderDelay(wIn), "tempRxMy", wIn)
					 << " <= X + (not Y) + '1';" <<endl;
			vhdl << tab << declare(getTarget()->adderDelay(wIn), "tempR"+son, wIn)
					 <<" <= "<< (opType_==SUBSUB ? "Y + (not X) + '1';" : "X + Y;") << endl;
			vhdl << tab << "XmY <= tempRxMy;" << endl;
			vhdl << tab << son << " <= tempR" << son << ";" << endl;
		}

		else		{
			
			// Here we split into chunks.
			double remainingSlack = targetPeriod-maxCP;
			int firstSubAdderSize = IntAdder::getMaxAdderSizeForPeriod(getTarget(), remainingSlack);
			int maxSubAdderSize = IntAdder::getMaxAdderSizeForPeriod(getTarget(), targetPeriod);

			bool loop=true;
			int subAdderSize=firstSubAdderSize;
			int previousSubAdderSize;
			int subAdderFirstBit = 0;
			int i=0;
            int skip_R0 = 0;
            // name the signals once for all. operations are 1 and 2
			string c1="Cin_XmY_";
			string c2="Cin_"+son + "_";
			string x1 = "X_";
			string y2 = "Y_";
			string y1 = "Y_";
			string x2 = (opType==SUBSUB?"mX_":"X_");
			string s1 = "S_XmY_";
			string s2 = "S_"+son+"_";
			string r1 = "t_XmY_";
			string r2 = "t_"+son+"_";
				
			while(loop) {
				REPORT(DETAILED, "Sub-adder " << i << " : first bit=" << subAdderFirstBit << ",  size=" <<  subAdderSize);
                if(0 < subAdderSize){
                    // Cin
                    if(subAdderFirstBit == 0)	{
                        vhdl << tab << declare(join(c1, i)) << " <= '1';" << endl; // X-Y is a subtraction
                        vhdl << tab << declare(join(c2, i)) << " <= " << (opType==SUBSUB? "'1'":"'0'" ) << ";" << endl;
                    }else	 {
                        vhdl << tab << declare(join(c1, i)) << " <= " << join(s1, i-1) <<	of(previousSubAdderSize) << ";" << endl;
                        vhdl << tab << declare(join(c2, i)) << " <= " << join(s2, i-1) <<	of(previousSubAdderSize) << ";" << endl;
                    }
                    // operands
                    vhdl << tab << declare(join("X_", i), subAdderSize) << " <= X"	<<	range(subAdderFirstBit+subAdderSize-1, subAdderFirstBit) << ";" << endl;
                    vhdl << tab << declare(join("Y_", i), subAdderSize) << " <= Y"	<<	range(subAdderFirstBit+subAdderSize-1, subAdderFirstBit) << ";" << endl;
                    vhdl << tab << declare(getTarget()->adderDelay(subAdderSize+1),
                                                                 join(s1, i),
                                                                 subAdderSize+1)
                             << " <= ('0' & X_" << i	<<	") + ('0' & not Y_" << i << ") + " << c1 << i << ";" << endl;
                    vhdl << tab << declare(getTarget()->adderDelay(subAdderSize+1),
                                                                 join(s2, i),
                                                                 subAdderSize+1)
                             << " <= ('0' & Y_" << i	<<	") + ('0' & " << (opType==SUBSUB? "not":"") << " X_" << i << ") + " << c2 << i << ";" << endl;

                    vhdl << tab << declare(join(r1, i), subAdderSize) << " <= " << s1 << i	<<	range(subAdderSize-1,0) << ";" << endl;
                    vhdl << tab << declare(join(r2, i), subAdderSize) << " <= " << s2 << i	<<	range(subAdderSize-1,0) << ";" << endl;
                } else {                                                                                                //do addition in the following cycle if there is unsufficient time in the current one
                    subAdderSize = 0;
                    subAdderFirstBit = 0;
                    skip_R0 = 1;
                    if(!maxSubAdderSize)
                    THROWERROR("Cannot realize IntAdder, because the target periode - FF-Delay (" << targetPeriod << ") is shorter than the adder Delay for a 1 bit adder (" << getTarget()->adderDelay(1) << ").");
                }

				// prepare next iteration
				i++;
				subAdderFirstBit += subAdderSize;
				previousSubAdderSize = subAdderSize;
				if (subAdderFirstBit==wIn)
					loop=false;
				else
					subAdderSize = min(wIn-subAdderFirstBit, maxSubAdderSize);
			}
			int ifinal=i;
			
			vhdl << tab << "XmY <= ";
			while(i>skip_R0)		{
				i--;
				vhdl << r1 << i << (i==skip_R0?" ":" & ");
			}
			vhdl << ";" << endl;

			i=ifinal;
			vhdl << tab << son << " <= ";
			while(i>skip_R0)		{
				i--;
				vhdl << r2 << i << (i==skip_R0?" ":" & ");
			}
			vhdl << ";" << endl;
		}
		//REPORT(DEBUG, "Exiting");
	}


	IntDualAddSub::~IntDualAddSub() {
	}

	void IntDualAddSub::emulate(TestCase* tc)
	{
		mpz_class svX = tc->getInputValue("X");
		mpz_class svY = tc->getInputValue("Y");

		mpz_class svRxMy = svX - svY;
		tc->addExpectedOutput("XmY", svRxMy);

		mpz_class svR2;
		if (opType_== SUBSUB)
			svR2=svY-svX;
		else {
			svR2=svX+svY;
			// Don't allow overflow
			mpz_clrbit(svR2.get_mpz_t(),wIn_);
		}
		tc->addExpectedOutput(son, svR2);
	}


	void IntDualAddSub::buildStandardTestCases(TestCaseList* tcl){
		TestCase *tc;

		tc = new TestCase(this);
		tc->addInput("X", mpz_class(0) );
		tc->addInput("Y", mpz_class(1));
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addInput("X", mpz_class(0) );
		tc->addInput("Y", mpz_class(-1));
		emulate(tc);
		tcl->add(tc);
	}



	TestList IntDualAddSub::unitTest(int index)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;
		
		if(index==-1) 
		{ // The unit tests
			
			for(int wIn=5; wIn<100; wIn+=30) // test various input widths
				{
					for(int opType = 0; opType <2; opType++) {
						paramList.push_back(make_pair("wIn", to_string(wIn)));
						paramList.push_back(make_pair("opType", to_string(opType)));
						testStateList.push_back(paramList);
						paramList.clear();
					}
				}
		}
		else     
			{
				// finite number of random test computed out of index
				// TODO ?
			}	

		return testStateList;
	}

	
	OperatorPtr IntDualAddSub::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int wIn;
		UserInterface::parseStrictlyPositiveInt(args, "wIn", &wIn);
		int opType;
		UserInterface::parsePositiveInt(args, "opType", &opType);
		return new IntDualAddSub(parentOp, target, wIn, opType);
	}

	void IntDualAddSub::registerFactory(){
		UserInterface::add("IntDualAddSub", // name
											 "Pipelined dual adder/subtractor",
											 "BasicInteger", // category
											 "",
											 "wIn(int): input size in bits;\
opType(int): 1=compute X-Y and X+Y, 2=compute X-Y and Y-X;",
											 "",
											 IntDualAddSub::parseArguments,
											 IntDualAddSub::unitTest
											 ) ;

	}
}
