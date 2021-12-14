/*
  A leading zero/one counter + shifter + sticky bit computer for FloPoCo

  Authors: Florent de Dinechin, Bogdan Pasca

   This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2011.
  All rights reserved.

*/

#include <iostream>
#include <sstream>
#include <vector>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "utils.hpp"
#include "Operator.hpp"
#include "LZOCShifterSticky.hpp"

using namespace std;



namespace flopoco{


	LZOCShifterSticky::LZOCShifterSticky(OperatorPtr parentOp, Target* target, int wIn, int wOut, int wCount, bool computeSticky, const int countType) :
		Operator(parentOp, target), wIn_(wIn), wOut_(wOut), wCount_(wCount), computeSticky_(computeSticky), countType_(countType) {

		// -------- Parameter set up -----------------
		srcFileName = "LZOCShifterSticky";
		setCopyrightString("Florent de Dinechin, Bogdan Pasca (2007-2016)");

		REPORT(DETAILED, "wIn="<<wIn << " wOut="<<wOut << " wCount="<<wCount << " computeSticky=" << computeSticky  << " countType=" << countType);

		ostringstream name;
		name << "L" << (countType_<0?"ZO":((countType_>0)?"O":"Z")) << "CShifter"
			  << (computeSticky_?"Sticky":"") << "_" << wIn_ << "_to_"<<wOut_<<"_counting_"<<(1<<wCount_);
		setNameWithFreqAndUID(name.str());

		addInput ("I", wIn_);
		if (countType_==-1) addInput ("OZb"); /* if we generate a generic LZOC */
		addOutput("Count", wCount_);
		addOutput("O", wOut_);
		if (computeSticky_)   addOutput("Sticky"); /* if we require a sticky bit computation */

		// we consider that wOut <= wIn. We fix this at the end if not the case
		int wOut_true = wOut_;
		wOut_ = wOut > wIn_ ? wIn_ : wOut;


		vhdl << tab << declare(join("level",wCount_), wIn_) << " <= I ;"   <<endl;
		if (countType_==-1) vhdl << tab << declare("sozb") << "<= OZb;"<<endl;
		if ((computeSticky_)&&(wOut_<wIn))   vhdl << tab << declare(join("sticky",wCount_)) << " <= '0' ;"<<endl; //init sticky


		// Now comes the main loop.
		// i is the level index. Level i counts 2^i bits, and shifts by 2^i
		int currLevSize=wIn, prevLevSize=0;
		for (int i=wCount_-1; i>=0; i--){
			prevLevSize = currLevSize;

			// level(k) = max ( max (2^k, wOut) + 2^k -1) , wIn)
			currLevSize = (wOut_>intpow2(i)?wOut_:intpow2(i));
			currLevSize += (intpow2(i)-1);
			currLevSize = (currLevSize > wIn_? wIn_: currLevSize);

			// Delay evaluation.
			// As we output the count bits, their computation will not be merged inside the shift
			//REPORT( DEBUG, "currSize="<<currLevSize);

			double countBitDelay = getTarget()->fanoutDelay(currLevSize);
			if (countType>=0)
				countBitDelay += getTarget()->eqConstComparatorDelay( intpow2(i) )  ;
			else
				countBitDelay += getTarget()->eqComparatorDelay( intpow2(i) ) ;
			
			vhdl << tab << declare(countBitDelay, join("count",i))
					 << "<= '1' when " <<join("level",i+1)<<range(prevLevSize-1,prevLevSize - intpow2(i))<<" = "
					 <<"("<<prevLevSize-1<<" downto "<<prevLevSize - intpow2(i)<<"=>"<< (countType_==-1? "sozb": countType_==0?"'0'":"'1'")<<") else '0';"<<endl;

			// The shift will take at most one LUT delay per level. We don't take into account that shift level can be merged: TODO ? It seems non-trivial.
			double shiftDelay = getTarget()->logicDelay(3);
			vhdl << tab << declare(shiftDelay,join("level",i),currLevSize)
					 << "<= " << join("level",i+1)<<"("<<prevLevSize-1<<" downto "<< prevLevSize-currLevSize << ")"
					 << " when " << join("count",i) << "='0' else ";
			int l,r;
			l = prevLevSize - intpow2(i) - 1;
			r = (currLevSize < prevLevSize - intpow2(i) ? (prevLevSize - intpow2(i)) - currLevSize : 0 );
			if (l>=r)
				vhdl << join("level",i+1) << "("<<prevLevSize - intpow2(i) - 1 <<" downto "<< (currLevSize < prevLevSize - intpow2(i) ? (prevLevSize - intpow2(i)) - currLevSize : 0 ) <<")";
			
			if (prevLevSize - intpow2(i) < currLevSize )
				vhdl << (l>=r?" & ":"") << rangeAssign(currLevSize -(prevLevSize - intpow2(i))-1,0,"'0'");
			vhdl << ";"<<endl;

			if ((computeSticky_)&&(wOut_<wIn)) {

				// Delay computation. Here we try to compute as much of the sticky in each level.
				double levelStickyDelay;
				// n is the size on which we compute the sticky bit
				int n = max( prevLevSize-currLevSize,
										 (currLevSize < prevLevSize - intpow2(i) ? (prevLevSize - int(intpow2(i)) ) - currLevSize : 0 ))  ;
				if ( countType_ == -1 )
					levelStickyDelay= getTarget()->eqComparatorDelay(n);
				else
					levelStickyDelay= getTarget()->eqConstComparatorDelay(n);
				

				vhdl << tab << declare(join("sticky_high_",i)) << "<= '0'";
				if (prevLevSize-currLevSize > 0)
					vhdl << "when " << join("level",i+1)<<"("<<prevLevSize-currLevSize -1 <<" downto "<< 0 <<") = CONV_STD_LOGIC_VECTOR(0,"<< prevLevSize-currLevSize <<") else '1'";
				vhdl << ";"<<endl;

   			vhdl << tab << declare(join("sticky_low_",i)) << "<= '0'";
				if ((currLevSize < prevLevSize - intpow2(i) ? (prevLevSize - intpow2(i)) - currLevSize : 0 ) > 0)
					vhdl << "when " <<join("level",i+1)<<"("<<(currLevSize < prevLevSize - intpow2(i) ? (prevLevSize - intpow2(i)) - currLevSize : 0 ) -1
						  <<" downto "<< 0 <<") = CONV_STD_LOGIC_VECTOR(0,"<< (currLevSize < prevLevSize - intpow2(i) ? (prevLevSize - intpow2(i)) - currLevSize : 0 ) <<") else '1'";
				vhdl << ";"<<endl;

				vhdl << tab << declare(levelStickyDelay, join("sticky",i))
						 << "<= " << join("sticky",i+1) << " or " << join("sticky_high_",i)
						 << " when " << join("count",i) << "='0' else " << join("sticky",i+1) << " or " << join("sticky_low_",i)<<";"<<endl;
			}

			vhdl <<endl;
		}

		//assign back the value to wOut_
		wOut_ =  wOut_true;
		vhdl << tab << "O <= "<< join("level",0)
			  << (wOut_<=wIn?"":join("&",rangeAssign(wOut_-wIn-1,0,"'0'")))<<";"<<endl;


		vhdl << tab << declare("sCount",wCount_) <<(wCount_==1?"(0)":"")<<" <= ";
		for (int i=wCount_-1; i>=0; i--){
			vhdl <<join("count",i);
			vhdl << (i>0?" & ":join(";","\n"));
		}

		if((1<<wCount_)-1 > wIn_) {
			vhdl << tab << "Count <= sCount;"<<endl;

			/*			vhdl << tab << "Count <= CONV_STD_LOGIC_VECTOR("<<wIn_<<","<<wCount_<<") when sCount=CONV_STD_LOGIC_VECTOR("<<intpow2(wCount_)-1<<","<<wCount_<<")"<<endl
				  << tab << tab << "else sCount;"<<endl;*/
		}
		else {
			vhdl << tab << "Count <= sCount;"<<endl;
		}


		if (computeSticky_){
			if (wOut_>=wIn)
				vhdl << tab << "Sticky <= '0';"<<endl;
			else
				vhdl << tab << "Sticky <= sticky0;"<<endl;
		}
		REPORT( DEBUG, "Leaving LZOCShifterSticky");

	}

	LZOCShifterSticky::~LZOCShifterSticky() {
	}


	int LZOCShifterSticky::getCountWidth() const{
		return wCount_;
	}



	void LZOCShifterSticky::emulate(TestCase* tc)
	{
		mpz_class inputValue  = tc->getInputValue("I");

		mpz_class sozb = 42; //dummy value
		if (countType_ == -1)
			sozb = tc->getInputValue("OZb");

		int sticky=0;
		int count =0;
		mpz_class shiftOutputValue = inputValue;


		mpz_class bit = (countType_ == -1) ? sozb : (countType_ == 0 ? 0 : 1); /* what are we counting in the specific case */

		int j=wIn_-1;
		while ((count < (1<<wCount_)-1) &&  (j>=0)  && mpz_tstbit(inputValue.get_mpz_t(), j) == bit)   {
			count ++;
			j--;
			shiftOutputValue = shiftOutputValue <<1;
			}

		// Now reformat the output value to its size, and compute the sticky of the remaining bits.
		// The max size of shiftOutputValue is ((1<<wCount)-1) + wIn
		mpz_class outputMask = (mpz_class(1) << wOut_) -1;
		int numBitsForSticky = wIn_ - wOut_;
		if(numBitsForSticky >= 0) {// should be the typical use case where we need to compute a sticky bit
			mpz_class stickyMask = (mpz_class(1) << numBitsForSticky) -1;
			mpz_class bitsForSticky = shiftOutputValue & stickyMask;
			sticky = (bitsForSticky==0? 0 :1 );
			shiftOutputValue = (shiftOutputValue >> numBitsForSticky) & outputMask;
		}
		else {
			shiftOutputValue = (shiftOutputValue << numBitsForSticky) & outputMask;
			sticky=0;
		}
			
		
		tc->addExpectedOutput("O", shiftOutputValue);
		tc->addExpectedOutput("Count", count);

		if (computeSticky_)
			tc->addExpectedOutput("Sticky",sticky);
	}


	

	OperatorPtr LZOCShifterSticky::parseArguments(OperatorPtr parentOp, Target *target, std::vector<std::string> &args) {
		int wIn, wOut, wCount, countType;
		bool computeSticky;
		UserInterface::parseStrictlyPositiveInt(args, "wIn", &wIn);
		UserInterface::parseStrictlyPositiveInt(args, "wOut", &wOut);
		UserInterface::parseStrictlyPositiveInt(args, "wCount", &wCount);
		UserInterface::parseBoolean(args, "computeSticky", &computeSticky);
		UserInterface::parseInt(args, "countType", &countType);
		return new LZOCShifterSticky(parentOp, target, wIn, wOut, wCount, computeSticky, countType);
	}


	
	void LZOCShifterSticky::registerFactory(){
		UserInterface::add("LZOCShifterSticky", // name
											 "A combined leading zero/one counter and shifter, useful for floating-point normalization.",
											 "ShiftersLZOCs",  // category
											 "", // see also
											 "wIn(int): input size in bits;\
                        wOut(int): output size in bits;\
                        wCount(int): size in bits of the count output;\
                        computeSticky(bool)=false: if false the shifted-out bits are discarded, if true they are ORed into a sticky bit which is output;\
                        countType(int)=-1:  0 to count zeroes, 1 to count ones, -1 to have a dynamic OZb input that tells what to count", // This string will be parsed
											 "", // no particular extra doc needed
											 LZOCShifterSticky::parseArguments
											 ) ;
		
	}


}
