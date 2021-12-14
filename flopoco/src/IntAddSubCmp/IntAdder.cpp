/*
An integer adder for FloPoCo

It may be pipelined to arbitrary frequency.
Also useful to derive the carry-propagate delays for the subclasses of Target

This is also the canonical example of an operator that pipelines itself according to the schedule of its parentOp.

Authors:  Bogdan Pasca, Florent de Dinechin, Matei Istoan

This file is part of the FloPoCo project
developed by the Arenaire team at Ecole Normale Superieure de Lyon

Initial software.
Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
2008-2010.
  All rights reserved.
*/

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "utils.hpp"
#include "IntAdder.hpp"

//#include "Attic/IntAdderClassical.hpp"
//#include "Attic/IntAdderAlternative.hpp"
//#include "Attic/IntAdderShortLatency.hpp"

using namespace std;
namespace flopoco {

	IntAdder::IntAdder (OperatorPtr parentOp, Target* target, int wIn_):
		Operator (parentOp, target), wIn ( wIn_ )
	{
		srcFileName="IntAdder";
		setCopyrightString ( "Bogdan Pasca, Florent de Dinechin (2008-2016)" );
		ostringstream name;
		name << "IntAdder_" << wIn;
		setNameWithFreqAndUID(name.str());
													
		// Set up the IO signals
		addInput  ("X"  , wIn, true);
		addInput  ("Y"  , wIn, true);
		addInput  ("Cin");
		addOutput ("R"  , wIn, 1 , true);

		double targetPeriod = 1.0/getTarget()->frequency() - getTarget()->ffDelay();
		// What is the maximum lexicographic time of our inputs?
		schedule();
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
			//REPORT(DEBUG, "1 " << getTarget()->adderDelay(wIn));
			vhdl << tab << declare(getTarget()->adderDelay(wIn),"Rtmp", wIn); // just to use declare()
			//REPORT(DEBUG, "2");
			vhdl << " <= X + Y + Cin;" << endl; 
			//REPORT(DEBUG, "3");
			vhdl << tab << "R <= Rtmp;" << endl;

		}

		else		{
            //cout << "----------totalPeriod" << totalPeriod << " targetPeriod " << targetPeriod << endl;
			// Here we split into chunks.
			double remainingSlack = targetPeriod-maxCP;                                                                 //remaining time to do addition in current period
			int firstSubAdderSize = getMaxAdderSizeForPeriod(getTarget(), remainingSlack);                       //remaining additions that can be performed in current period
			int maxSubAdderSize = getMaxAdderSizeForPeriod(getTarget(), targetPeriod);                           //total additions that can be performed in a period
            //cout << "----------SubAdderSize" << firstSubAdderSize << " MaxSubAdderSize " << maxSubAdderSize << " Adder Delay n1 " << getTarget()->adderDelay(1) << " Adder Delay n2 " << getTarget()->adderDelay(2) << " remainingSlack " << remainingSlack << endl;
			bool loop=true;
			int subAdderSize=firstSubAdderSize;                                                                         //the size of the first sub-adder can be as large as there is time left to do the addition in the current period
			int previousSubAdderSize=0;                                                                                 //keep record of the bits already added for the next iteration
			int subAdderFirstBit = 0;                                                                                   //Bit 0 is added first
			int i=0;                                                                                                    //cycle counter
			int skip_R0 = 0;                                                                                            //skip the result form the first cycle if there was not enough time to do an addition of at leased size one
			while(loop) {
				REPORT(DETAILED, "Sub-adder " << i << " : first bit=" << subAdderFirstBit << ",  size=" <<  subAdderSize);
				if(0 < subAdderSize){                                                                                   //there is time to do at least an addition of bit-width one in current cycle
                    // Cin
                    if(subAdderFirstBit == 0)	{                                                                       //handle carry-in in first cycle
                        vhdl << tab << declare(join("Cin_", i)) << " <= Cin;" << endl;
                    }else	 {                                                                                              //handle carry-in in subsequent cycles
                        vhdl << tab << declare(join("Cin_", i)) << " <= " << join("S_", i-1) <<	of(previousSubAdderSize) << ";" << endl;
                    }
                    // operands
				    vhdl << tab << declare(join("X_", i), subAdderSize+1) << " <= '0' & X"	<<	range(subAdderFirstBit+subAdderSize-1, subAdderFirstBit) << ";" << endl;
                    vhdl << tab << declare(join("Y_", i), subAdderSize+1) << " <= '0' & Y"	<<	range(subAdderFirstBit+subAdderSize-1, subAdderFirstBit) << ";" << endl;
                    vhdl << tab << declare(getTarget()->adderDelay(subAdderSize+1), join("S_", i), subAdderSize+1)
                         << " <= X_" << i	<<	" + Y_" << i << " + Cin_" << i << ";" << endl;
                    vhdl << tab << declare(join("R_", i), subAdderSize) << " <= S_" << i	<<	range(subAdderSize-1,0) << ";" << endl;
				} else {                                                                                                //do addition in the following cycle if there is unsufficient time in the current one
                    subAdderSize = 0;
                    subAdderFirstBit = 0;
                    skip_R0 = 1;
                    if(!maxSubAdderSize)
                        THROWERROR("Cannot realize IntAdder, because the target periode - FF-Delay (" << targetPeriod << ") is shorter than the adder Delay for a 1 bit adder (" << getTarget()->adderDelay(1) << ").");
				}

				// prepare next iteration
				i++;
				subAdderFirstBit += subAdderSize;                                                                       //add MSBits in next cycle
				previousSubAdderSize = subAdderSize;                                                                    //keep track of the MSB in current cycle to handle carry for next cycle
				if (subAdderFirstBit==wIn)                                                                              //no further cycle is needed if all bits are already added
					loop=false;
				else
					subAdderSize = min(wIn-subAdderFirstBit, maxSubAdderSize);                                       //the width of the sub-adder in the next cycle is either the remaining bits, or the maximal number of bits that can be added in one cycle, if there is no time to add all remaining bits in one cycle
			}

			vhdl << tab << "R <= ";
			while(i>skip_R0)		{                                                                                   //calculate the result as the concatenation of the sub results, skip the first sub-result if there was no first result
				i--;
				vhdl <<  "R_" << i << (i==skip_R0?" ":" & ");
			}
			vhdl << ";" << endl;
		}
		//REPORT(DEBUG, "Exiting");

	}


	int IntAdder::getMaxAdderSizeForPeriod(Target* target, double targetPeriod) {
		int count = 1;                                                                                                  // Start checking the addition width that can be performed int the remaining time in the current cycle at 1-bit
        while(target->adderDelay(count) < targetPeriod){
            count++;
		}
		return count-1;
	}

	/*************************************************************************/
	IntAdder::~IntAdder() {
	}

	/*************************************************************************/
	void IntAdder::emulate ( TestCase* tc ) {
		// get the inputs from the TestCase
		mpz_class svX = tc->getInputValue ( "X" );
		mpz_class svY = tc->getInputValue ( "Y" );
		mpz_class svC = tc->getInputValue ( "Cin" );

		// compute the multiple-precision output
		mpz_class svR = svX + svY + svC;
		// Don't allow overflow: the output is modulo 2^wIn
		svR = svR & ((mpz_class(1)<<wIn)-1);

		// complete the TestCase with this expected output
		tc->addExpectedOutput ( "R", svR );
	}


	OperatorPtr IntAdder::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int wIn;
		UserInterface::parseStrictlyPositiveInt(args, "wIn", &wIn, false);
		return new IntAdder(parentOp, target, wIn);
	}

	void IntAdder::registerFactory(){
		UserInterface::add("IntAdder", // name
											 "Integer adder. In modern VHDL, integer addition is expressed by a + and one usually needn't define an entity for it. However, this operator will be pipelined if the addition is too large to be performed at the target frequency.",
											 "BasicInteger", // category
											 "",
											 "wIn(int): input size in bits;\
					  arch(int)=-1: -1 for automatic, 0 for classical, 1 for alternative, 2 for short latency; \
					  optObjective(int)=2: 0 to optimize for logic, 1 to optimize for register, 2 to optimize for slice/ALM count; \
					  SRL(bool)=true: optimize for shift registers",
											 "",
											 IntAdder::parseArguments
											 );
		
	}


}


