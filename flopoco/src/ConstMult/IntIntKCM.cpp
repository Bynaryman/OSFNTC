/*
 * An constant multiplier for FloPoCo using the KCM method
  This file is part of the FloPoCo project developed by the Arenaire
  team at Ecole Normale Superieure de Lyon

  Author :  Bogdan.Pasca, Florent.de.Dinechin, both @ens-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2010, 2019
  All rights reserved.

 */

// TODO if LUTsize=5 and wIn=11, we should have 2 tables, not 3

#include <iostream>
#include <sstream>
#include <vector>
#include <typeinfo>
#include <math.h>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "../utils.hpp"
#include "../Operator.hpp"
#include "../IntAddSubCmp/IntAdder.hpp"
#include "IntIntKCM.hpp"
#include "KCMTable.hpp"

using namespace std;

namespace flopoco{

	IntIntKCM::IntIntKCM(OperatorPtr parentOp, Target* target, int wIn, mpz_class C, bool inputTwosComplement):
		Operator(parentOp, target), wIn_(wIn), signedIn_(inputTwosComplement), C_(C)
	{
		setCopyrightString("Bogdan Pasca, Florent de Dinechin (2009,2010)");
		srcFileName="IntIntKCM";

		// Set up the IO signals
		addInput ("X" , wIn_);

		if(C<0)
			throw string("IntIntKCM: only positive constants are supported");

		wOut_ = intlog2(C) + wIn_;

		addOutput("R" , wOut_);

		ostringstream name;
		name << "IntIntKCM_" << wIn_ << "_" << C << (signedIn_?"_signed":"_unsigned");
		setNameWithFreqAndUID(name.str());

		int constantWidth = intlog2( C );
		int lutWidth = getTarget()->lutInputs();
		chunkSize_ = lutWidth;
		int nbOfTables = int ( ceil( double(wIn)/double(lutWidth)) );
		int lastLutWidth = (wIn%lutWidth==0 ? lutWidth : wIn%lutWidth);

		// Better to double an existing table than adding one more table and one more addition.
		if(lastLutWidth==1)
		{
		  nbOfTables--;
		  lastLutWidth=lutWidth + 1;
		}

		// Actually there are only two cases:
		// if lastLutWidth<=lutWidth, all the digits (including the last one) may have the same size,
		// we just pad the last digit with zeroes and use the same table as for the other digits.
		// now if lastLutWidth=lutWidth + 1  then we need a larger last digit, and a different last table.
		// Besides if signedResult we also need a different last table.

		REPORT(INFO, "Constant multiplication in "<< nbOfTables << " tables, input sizes:  for the first tables, " << lutWidth << ", for the last table: " << lastLutWidth);

		// TODO		setCriticalPath( getMaxInputDelays(inputDelays) );

		// First get rid of the easy case when there is just one table
		if(nbOfTables==1)
		{
			KCMTable  *lastTable=0;
			lastTable = new KCMTable(target, wIn_, constantWidth + wIn_, C, signedIn_);
			useSoftRAM(lastTable);

			// pipeline depth of a Table, so far, is always 0, but the table is well behaved and updates the critical path.
			double tableDelay=lastTable->getOutputDelay("Y"); // since we passed an empty inputDelayMap

			manageCriticalPath(tableDelay);
			inPortMap (lastTable, "X", "X");
			outPortMap(lastTable, "Y", "Ri");
			vhdl << instance(lastTable, "KCMTable");

			vhdl << tab << "R <= Ri;" <<endl;
		}
		else
		{
			///////////////////////////////////   Generic Case  ////////////////////////////////////

			KCMTable *firstTable=0, *lastTable=0;

			firstTable = new KCMTable(target, lutWidth, constantWidth + lutWidth, C, false);
			useSoftRAM(firstTable);

			lastTable = new KCMTable(target, lastLutWidth, constantWidth + lastLutWidth, C, signedIn_);
			useSoftRAM(lastTable);

			// Critical path among the tables is through the last one, which may be larger
			double tableDelay=lastTable->getOutputDelay("Y"); // since we passed an empty inputDelayMap

			//possibly register the inputs before the table read
			manageCriticalPath(tableDelay);

			//create the tables
			for (int i=0; i<nbOfTables; i++)
			{
				//first split the input X into digits having lutWidth bits
				KCMTable *t;

				if(i < nbOfTables-1)
				{
					vhdl << tab << declare( join("d",i), lutWidth ) << " <= X" << range(lutWidth*(i+1)-1, lutWidth*i ) << ";" <<endl;
					t=firstTable;
				}
				else
				{
					// last table is a bit special
					vhdl << tab << declare( join("d",i), lastLutWidth ) << " <= " << "X" << range( wIn-1 , lutWidth*i ) << ";" <<endl;
					t=lastTable;
				}

				inPortMap (t , "X", join("d",i));
				outPortMap(t , "Y", join("pp",i));
				vhdl << instance(t , join("KCMTable_",i));
			}




			if(nbOfTables==2)
				{ // use a simple adder
					//determine the addition operand size
					int addOpSize = (nbOfTables - 2) * lutWidth  +  lastLutWidth + constantWidth;
					for(int i=0; i<nbOfTables; i++)
						{
							vhdl << tab << declare( join("addOp",i), addOpSize ) << " <= ";
							if(i!=nbOfTables-1)
								{
									//if not the last table
									vhdl << rangeAssign(addOpSize-1, constantWidth + i*lutWidth, "'0'") << " & "
											 <<  join("pp",i) << range(constantWidth + lutWidth -1, (i==0?lutWidth:0)) << " & "
											 << zg((i-1)*lutWidth,0) << ";" << endl;
								}
							else
								{
									vhdl << join("pp",i)<<range(constantWidth + lastLutWidth -1, (i==0?lutWidth:0))<< " & " << zg((i-1)*lutWidth,0) << ";" << endl;
								}
						}

					Operator* adder;
					adder = new IntAdder(target, addOpSize);
					addSubComponent(adder);
					inPortMap (adder, "X" , join("addOp",0));
					inPortMap (adder, "Y" , join("addOp",1));
					inPortMapCst(adder, "Cin" , "'0'");
					outPortMap(adder, "R", "OutRes");
					vhdl << instance(adder, "Result_Adder");
					syncCycleFromSignal("OutRes");

					getOutDelayMap()["R"] = adder->getOutputDelay("R");
					vhdl << tab << "R <= OutRes & pp0" << range(lutWidth-1,0) << ";" <<endl;
				}


			else // nbOfTables >2,  use a bit heap
				{
				//create the bitheap
				bitHeap = new BitHeap(this, wOut_);

				//add the results of all but the last table to the bitheap
				for(int i=0; i<nbOfTables-1; i++)
				{
					int tableWeightShift = i*lutWidth;

					//add the bits to the bit heap
					for(int w=0; w<=constantWidth + lutWidth-1; w++)
					{
						stringstream s;
						s << "pp" << i << of(w);
						bitHeap->addBit(w+tableWeightShift, s.str());
					}
				}

				//add the result of the last table to the bitheap
				{
					int tableWeightShift = (nbOfTables-1)*lutWidth;

					for(int w=0; w<=constantWidth + lastLutWidth-1; w++)
					{
						stringstream s;

						if((w == constantWidth+lastLutWidth-1) && signedIn_)
							s << "not(pp" << nbOfTables-1 << of(w) << ")";
						else
							s << "pp" << nbOfTables-1 << of(w);

						bitHeap->addBit(w+tableWeightShift, s.str());

						if((w == constantWidth+lastLutWidth-1) && signedIn_)
						{
							for(int w2=w; w2<=wOut_; w2++)
								bitHeap->addConstantOneBit(w2+tableWeightShift);
						}
					}
				}

				//compress the bitheap and produce the result
				bitHeap->generateCompressorVHDL();

				//manage the pipeline
				syncCycleFromSignal(bitHeap->getSumName());

				//because of final add in bit heap, add one more bit to the result
				vhdl << declare("OutRes", wOut_) << " <= " << bitHeap->getSumName() << range(wOut_-1, 0) << ";" << endl;

				vhdl << tab << "R <= OutRes;" <<endl;
				getOutDelayMap()["R"] = getCriticalPath();
			}
		}
	}


	//operator incorporated into a global compression
	//	for use as part of a bigger operator
	IntIntKCM::IntIntKCM(Operator* parentOp_, Target* target, Signal* multiplicandX, int wIn, mpz_class C, bool inputTwosComplement, BitHeap* bitHeap_):
		Operator(target), wIn_(wIn), signedIn_(inputTwosComplement), C_(C),
			bitHeap(bitHeap_), parentOp(parentOp_)
	{
		setCopyrightString("Bogdan Pasca, Florent de Dinechin (2009,2010)");
		srcFileName="IntIntKCM";

		if(C<0)
			throw string("IntIntKCM: only positive constants are supported");

		wOut_ = intlog2(C) + wIn_;

		ostringstream name;
		name << "IntIntKCM_" << wIn_ << "_" << C << (signedIn_ ? "_signed" : "_unsigned");
		setNameWithFreqAndUID(name.str());

		int constantWidth = intlog2(C);
		int lutWidth = getTarget()->lutInputs();
		chunkSize_ = lutWidth;
		int nbOfTables = int ( ceil(double(wIn)/double(lutWidth)) );
		int lastLutWidth = (wIn%lutWidth==0 ? lutWidth : wIn%lutWidth);

		// Better to double an existing table than adding one more table and one more addition.
		if(lastLutWidth==1)
		{
		  nbOfTables--;
		  lastLutWidth=lutWidth + 1;
		}

		// Actually there are only two cases:
		// 	if lastLutWidth <= lutWidth, all the digits (including the last one) may have the same size,
		// 	we just pad the last digit with zeroes and use the same table as for the other digits.
		// 	now if lastLutWidth=lutWidth + 1  then we need a larger last digit, and a different last table.
		// Besides if signedResult we also need a different last table.

		REPORT(INFO, "Constant multiplication in " << nbOfTables << " tables, input sizes:  for the first tables, " << lutWidth << ", for the last table: " << lastLutWidth);

		parentOp->setCycleFromSignal(multiplicandX->getName());

		// First get rid of the easy case when there is just one table
		if(nbOfTables==1)
		{
			KCMTable *lastTable=0;
			lastTable = new KCMTable(target, wIn_, constantWidth + wIn_, C, signedIn_);
			parentOp->addSubComponent(lastTable);
			useSoftRAM(lastTable);

			// pipeline depth of a Table, so far, is always 0, but the table is well behaved and updates the critical path.
			double tableDelay=lastTable->getOutputDelay("Y"); // since we passed an empty inputDelayMap

			parentOp->manageCriticalPath(tableDelay);
			parentOp->inPortMap (lastTable, "X", multiplicandX->getName());
			parentOp->outPortMap(lastTable, "Y", join("Ri", "_intKcmMult_", getuid()));
			parentOp->vhdl << parentOp->instance(lastTable, join("IntKCMTable_0_intKcmMult_", getuid()));

			//add the bits to the bit heap
			for(int w=0; w<=constantWidth+wIn_-1; w++)
			{
				stringstream s;

				if((w == constantWidth+lastLutWidth-1) && signedIn_)
					s << "not(Ri" << "_intKcmMult_" << getuid() << of(w) << ")";
				else
					s << "Ri" << "_intKcmMult_" << getuid() << of(w);

				bitHeap->addBit(w, s.str());
			}

			//sign extend if necessary
			if(signedIn_)
			{
				for(int w=constantWidth+wIn_-1; w<=(int)bitHeap->getMaxWeight()-(int)bitHeap->getMinWeight(); w++)
					bitHeap->addConstantOneBit(w);
			}
		}
		else
		{
			///////////////////////////////////   Generic Case  ////////////////////////////////////

			KCMTable *firstTable=0, *lastTable=0;

			firstTable = new KCMTable(target, lutWidth, constantWidth + lutWidth, C, false);
			parentOp->addSubComponent(firstTable);
			useSoftRAM(firstTable);

			lastTable = new KCMTable(target, lastLutWidth, constantWidth + lastLutWidth, C, signedIn_);
			parentOp->addSubComponent(lastTable);
			useSoftRAM(lastTable);

			// Critical path among the tables is through the last one, which may be larger
			double tableDelay=lastTable->getOutputDelay("Y"); // since we passed an empty inputDelayMap

			//possibly register the inputs before the table read
			parentOp->manageCriticalPath(tableDelay);

			for (int i=0; i<nbOfTables; i++)
			{
				//first split the input X into digits having lutWidth bits
				KCMTable *t;

				if(i < nbOfTables-1)
				{
					parentOp->vhdl << tab << parentOp->declare( join("d",i), lutWidth ) << " <= " << multiplicandX->getName() << range(lutWidth*(i+1)-1, lutWidth*i ) << ";" <<endl;
					t = firstTable;
				}
				else
				{
					// last table is a bit special
					parentOp->vhdl << tab << declare( join("d",i), lastLutWidth ) << " <= " << multiplicandX->getName() << range( wIn-1 , lutWidth*i ) << ";" <<endl;
					t = lastTable;
				}

				parentOp->inPortMap (t , "X", join("d", i, "_intKcmMult_", getuid()));
				parentOp->outPortMap(t , "Y", join("pp", i, "_intKcmMult_", getuid()));
				parentOp->vhdl << parentOp->instance(t , join("IntKCMTable_", i, "_intKcmMult_", getuid()));
			}

			//add the results of all but the last table to the bitheap
			for(int i=0; i<nbOfTables-1; i++)
			{
				int tableWeightShift = i*lutWidth;

				//add the bits to the bit heap
				for(int w=0; w<=constantWidth + lutWidth-1; w++)
				{
					stringstream s;

					s << "pp" << i << "_intKcmMult_" << getuid() << of(w);

					bitHeap->addBit(w+tableWeightShift, s.str());
				}
			}

			//add the result of the last table to the bitheap
			{
				int tableWeightShift = (nbOfTables-1)*lutWidth;

				for(int w=0; w<=constantWidth + lastLutWidth-1; w++)
				{
					stringstream s;

					if((w == constantWidth+lastLutWidth-1) && signedIn_)
						s << "not(pp" << nbOfTables-1 << "_intKcmMult_" << getuid() << of(w) << ")";
					else
						s << "pp" << nbOfTables-1 << "_intKcmMult_" << getuid() << of(w);

					bitHeap->addBit(w+tableWeightShift, s.str());

					if((w == constantWidth+lastLutWidth-1) && signedIn_)
					{
						for(int w2=w; w2<=(int)(bitHeap->getMaxWeight()-bitHeap->getMinWeight()); w2++)
							bitHeap->addConstantOneBit(w2+tableWeightShift);
					}
				}
			}
		}
	}

	IntIntKCM::~IntIntKCM() {
	}

	int IntIntKCM::getOutputWidth(){
		return wOut_;
	}

	void IntIntKCM::emulate(TestCase* tc)
	{
		mpz_class svX =  tc->getInputValue("X");
		// cout << "-------------------------"<<endl;
		// cout << "X="<<unsignedBinary(svX, wIn_);
		// bool xneg = false;
		//x is in 2's complement, so it's value is
		if(signedIn_) {
			if ( svX > ( (mpz_class(1)<<(wIn_-1))-1) ){
				// cout << "X is negative" << endl;
				// xneg = true;
				svX = svX - (mpz_class(1)<<wIn_);
			}
		}
		mpz_class svR;

		svR = svX * C_;

		if ( svR < 0)
			svR = (mpz_class(1)<<wOut_) + svR;

		// cout << "R="<<unsignedBinary(svR, wOut_);

		tc->addExpectedOutput("R", svR);
	}

	
	TestList FixRealKCM::unitTest(int index)
	{
		// the static list of mandatory tests
		TestList testStateList;
			
		if(index==-1) 
		{ // The unit tests

			// TODO!
		}
		else     
		{
				// finite number of random test computed out of index
		}	

		return testStateList;
	}

	OperatorPtr FixRealKCM::parseArguments(OperatorPtr parentOp, Target* target, std::vector<std::string> &args)
	{
		int lsbIn, lsbOut, msbIn;
		bool signedIn;
		double targetUlpError;
		string constant;
		UserInterface::parseInt(args, "lsbIn", &lsbIn);
		UserInterface::parseString(args, "constant", &constant);
		UserInterface::parseInt(args, "lsbOut", &lsbOut);
		UserInterface::parseInt(args, "msbIn", &msbIn);
		UserInterface::parseBoolean(args, "signedIn", &signedIn);
		UserInterface::parseFloat(args, "targetUlpError", &targetUlpError);	
		return new FixRealKCM(
													parentOp,
													target, 
													signedIn,
													msbIn,
													lsbIn,
													lsbOut,
													constant, 
													targetUlpError
													);
	}

	void FixRealKCM::registerFactory()
	{
		UserInterface::add(
				"FixRealKCM",
				"Table based real multiplier. Output size is computed",
				"ConstMultDiv",
				"",
				"signedIn(bool): 0=unsigned, 1=signed; \
				msbIn(int): weight associated to most significant bit (including sign bit);\
				lsbIn(int): weight associated to least significant bit;\
				lsbOut(int): weight associated to output least significant bit; \
				constant(string): constant given in arbitrary-precision decimal, or as a Sollya expression, e.g \"log(2)\"; \
				targetUlpError(real)=1.0: required precision on last bit. Should be strictly greater than 0.5 and lesser than 1;",
				"This variant of Ken Chapman's Multiplier is briefly described in <a href=\"bib/flopoco.html#DinIstoMas2014-SOPCJR\">this article</a>.<br> Special constants, such as 0 or powers of two, are handled efficiently.",
				FixRealKCM::parseArguments,
				FixRealKCM::unitTest
		);
	}

	
}
