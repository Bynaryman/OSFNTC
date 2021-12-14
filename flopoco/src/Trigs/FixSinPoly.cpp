/*
 * Operator computing X- (X^3)/6, for X being a signed two's complement number, purely in logic (no mult)
 This operator uses one single truncated bit heap
 Assumptions: 0<=X<1

 This file is part of the FloPoCo project developed by the Arenaire/ARIC
 team at Ecole Normale Superieure de Lyon

 Author : Florent de Dinechin, Matei Istoan

 Initial software.
 Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
 2008-2013.
  All rights reserved.
*/

#include "FixSinPoly.hpp"
#include "ConstDiv3ForSinPoly.hpp"


using namespace std;

namespace flopoco{


	/* This class was written at a time when the bit heap framework only supported integers, not general fix-point numbers.
	 Therefore there are implicit scaling factors all over the place, and it is unreadable.
	 It could be refactored to be quite simpler */
	
	//FIXME: for now, the output width (wOut) is computed inside the operator,
	//		and not from the parameters given to the constructor

	//standalone operator
	FixSinPoly::FixSinPoly(OperatorPtr parentOp, Target* target, int msbIn_, int lsbIn_, bool truncated_, int msbOut_, int lsbOut_, bool signedIn_) :
		Operator(parentOp, target), msbIn(msbIn_), lsbIn(lsbIn_), truncated(truncated_), msbOut(msbOut_), lsbOut(lsbOut_), signedIn(signedIn_)
	{
		int indexMin, indexMax, wOutFull;

		srcFileName="FixSinPoly";

		if(lsbIn > msbIn)
			throw string("FixSinPoly: Error, lsbIn should not be greater than msbIn");

		if(truncated && (lsbOut > msbOut))
			throw string("FixSinPoly: Error, lsbOut should not be greater than msbOut");

		if(!truncated)
		{
			msbOut = msbIn;
			lsbOut = (lsbIn<0 ? 3*lsbIn : lsbIn) - 1;
		}

		wIn = msbIn - lsbIn + 1;
		if(truncated)
		{
			//compute the number of needed guard bits
			g = neededGuardBits(lsbOut);

			wOut = (msbIn<0 ? msbIn : 3*msbIn) - lsbOut + 1;
			wOutFull = (msbIn<0 ? msbIn : 3*msbIn) - (lsbIn<0 ? 3*lsbIn : lsbIn) + 1 + 1;			//one more bit to cover all the range of produced bits

			REPORT(DEBUG, "wIn=" << wIn << " wOut=" << wOut << " msbIn=" << msbIn << " lsbIn=" << lsbIn << " msbOut=" << msbOut << " lsbOut=" << lsbOut << " g=" << g);
		}
		else
		{
			//no guard bits needed
			g = 0;

			wOut = (msbIn<0 ? msbIn : 3*msbIn) - (lsbIn<0 ? 3*lsbIn : lsbIn) + 1 + 1;				//one more bit to cover all the range of produced bits

			REPORT(DEBUG, "wIn=" << wIn << " wOut=" << wOut << " msbIn=" << msbIn << " lsbIn=" << lsbIn << " msbOut=" << msbOut << " lsbOut=" << lsbOut);
		}

		// build the name
		ostringstream name;
		name << "FixSinPoly_" << vhdlize(wIn) << "_" << vhdlize(wOut) << (truncated ? "_truncated" : "") << (signedIn ? "_signed" : "_unsigned");
		setNameWithFreqAndUID(name.str());

		//create the input and the output
		addInput("X", wIn);
		addOutput("R", wOut);

		//create the bitheap that computes the sum
		bitHeap = new BitHeap(this, wOut+g);

		REPORT(DEBUG, "Adding the bits for X");

		//add the bits corresponding to sum_{i=imin}^imax(2^i*x_i)
		indexMin = (truncated ? (lsbOut-g>lsbIn ? lsbOut-g : lsbIn) : lsbIn);
		indexMax = msbIn;
		for(int i=indexMin; i<=indexMax; i++)
		{
			stringstream s;

			vhdl << tab << declare(join("X_orig_", (indexMin<0 ? i-indexMin + (lsbOut-g>lsbIn ? lsbOut-g-lsbIn : 0) : i)))
				<< " <= X" << of(indexMin<0 ? i-indexMin + (lsbOut-g>lsbIn ? lsbOut-g-lsbIn : 0) : i) << ";" << endl;

			s << join("X_orig_", (indexMin<0 ? (lsbOut-g>lsbIn ? i-indexMin+(lsbOut-g-lsbIn) : i-lsbIn) : i));

			if(indexMax<0 && indexMin<0)
			{
				bitHeap->addBit(s.str(), wOut + g - 1 - (indexMax - i));
			}
			else if(indexMin<0)
			{
				bitHeap->addBit(s.str(), (i - indexMin) - 2 * indexMin + 1);
			}
			else
			{
				bitHeap->addBit(s.str(), i);
			}
		}


		REPORT(DEBUG, "Adding the bits for the first sum");
		//add the terms corresponding to sum_{i=imin}^imax(2^(3i-1)*x_i)
		//	negated
		ConstDiv3ForSinPoly *divider;
		// A unique instance without exposing the interface
		schedule();
		inPortMap ("X", "X");
		outPortMap("Q", "XZeroIntDiv3");
		divider = new ConstDiv3ForSinPoly(this, target, wIn, 3, -1, 2, false) ;
		vhdl << instance(divider , "Divider");

		//manage the pipeline

		indexMax = ((msbIn-lsbIn+1)*3-2)-1;
		indexMin = (truncated ? wOutFull-1-(msbIn-lsbOut+g) : 0);
		for(int i=indexMin; i<=indexMax; i++)
		{
			stringstream s;

			vhdl << tab << declare(getTarget()->logicDelay(), join("XZeroIntDiv3_inverted_", i)) << " <= not XZeroIntDiv3" << of(i) << ";" << endl;

			s << "XZeroIntDiv3_inverted_" << i;

			bitHeap->addBit(s.str(), (truncated ? i - (wOutFull - (msbIn - lsbOut + 1 + g)) : i));

			for(int j=(truncated ? i-(wOutFull-(msbIn-lsbOut+1+g)) : i); j<wOut+g; j++)
				bitHeap->addConstantOneBit(j);
		}

		REPORT(DEBUG, "Adding the bits for the second sum");

		//add the terms corresponding to sum_i_j_imin^imax(2^(i+2j-1)*x_i*x_j)
		//	negated
		for(int i=lsbIn; i<=msbIn; i++)
			for(int j=lsbIn; j<=msbIn; j++)
			{
				//add the bit only if i != j
				if(i == j)
					continue;
				//if truncated, then add the bit only if it is in the correct bit-range
				if(truncated && ((lsbIn<0 ? (i-lsbIn)+2*(j-lsbIn)-1 : i+2*j-1)+1 < wOutFull-1-(msbIn-lsbOut+g)))
					continue;

				stringstream s;

				vhdl << tab << declare(getTarget()->logicDelay(), join("X_temp_", (lsbIn<0 ? i-lsbIn : i), "_", (lsbIn<0 ? j-lsbIn : j)))
					<< " <= not (X" << of(lsbIn<0 ? i-lsbIn : i) << " and X" << of(lsbIn<0 ? j-lsbIn : j) << ");" << endl;

				s << "X_temp_" << (lsbIn<0 ? i-lsbIn : i) << "_" << (lsbIn<0 ? j-lsbIn : j);

				bitHeap->addBit(s.str(), (lsbIn < 0 ? (i - lsbIn) + 2 * (j - lsbIn) - 1 : i + 2 * j - 1) + 1 -
										 (truncated ? (wOutFull - (msbIn - lsbOut + 1 + g)) : 0));

				for(int k=(lsbIn<0 ? (i-lsbIn)+2*(j-lsbIn)-1 : i+2*j-1) + 1 - (truncated ? (wOutFull-(msbIn-lsbOut+1+g)) : 0); k<wOut+g; k++)
					bitHeap->addConstantOneBit(k);
			}

		REPORT(DEBUG, "Adding the bits for the third sum");


		//add the terms corresponding to sum_i_j_k_imin^imax(2^(i+j+k)*x_i*x_j*x_k)
		//	negated
		for(int i=lsbIn; i<=msbIn; i++)
			for(int j=i+1; j<=msbIn; j++)
				for(int k=j+1; k<=msbIn; k++)
				{
					//if truncated, then add the bit only if it is in the correct bit-range
					if(truncated && ((lsbIn<0 ? (i-lsbIn)+(j-lsbIn)+(k-lsbIn) : i+j+k)+1 < wOutFull-1-(msbIn-lsbOut+g)))
						continue;

					//i < j < k
					stringstream s;

					vhdl << tab << declare(getTarget()->logicDelay(), join("X_temp2_", (lsbIn<0 ? i-lsbIn : i), "_", (lsbIn<0 ? j-lsbIn : j), "_", (lsbIn<0 ? k-lsbIn : k)))
						<< " <= not (X" << of(lsbIn<0 ? i-lsbIn : i) << " and X" << of(lsbIn<0 ? j-lsbIn : j) << " and X" << of(lsbIn<0 ? k-lsbIn : k) << ");" << endl;

					s << "X_temp2_" << (lsbIn<0 ? i-lsbIn : i) << "_" << (lsbIn<0 ? j-lsbIn : j) << "_" << (lsbIn<0 ? k-lsbIn : k);

					bitHeap->addBit(s.str(), (lsbIn < 0 ? (i - lsbIn) + (j - lsbIn) + (k - lsbIn) : i + j + k) + 1 -
											 (truncated ? (wOutFull - (msbIn - lsbOut + 1 + g)) : 0));

					for(int l=(lsbIn<0 ? (i-lsbIn)+(j-lsbIn)+(k-lsbIn) : i+j+k) + 1- (truncated ? (wOutFull-(msbIn-lsbOut+1+g)) : 0); l<wOut+g; l++)
						bitHeap->addConstantOneBit(l);
				}

		//if needed add a bit at position g-1, for rounding
		if(truncated)
			bitHeap->addConstantOneBit(g-1);

		//compress the bitheap
		bitHeap -> startCompression();

		//generate the final result
		if(truncated)
			vhdl << tab << "R" << " <= " << bitHeap-> getSumName() << range(wOut+g-1, g) << ";" << endl;
		else
			vhdl << tab << "R" << " <= " << bitHeap-> getSumName() << range(wOut-1, 0) << ";" << endl;
	}


	//operator incorporated into a global compression
	//	for use as part of a bigger operator
	FixSinPoly::FixSinPoly(Operator* parentOp_, Target* target, Signal* multiplicandX, int msbIn_, int lsbIn_, int truncated_, int msbOut_, int lsbOut_, BitHeap* bitHeap_, bool signedIn_) :
		Operator(parentOp_, target), msbIn(msbIn_), lsbIn(lsbIn_), truncated(truncated_), msbOut(msbOut_), lsbOut(lsbOut_),
		wIn(msbIn_-lsbIn+1), wOut(msbOut_-lsbOut+1), signedIn(signedIn_), bitHeap(bitHeap_), parentOp(parentOp_)
	{
		srcFileName="FixSinPoly";

		// build the name
		ostringstream name;
		name <<"FixSinPoly_" << vhdlize(wIn) << "_" << vhdlize(wOut) << "_" << (signedIn?"_signed":"_unsigned");
		setNameWithFreqAndUID(name.str());


	}




	FixSinPoly::~FixSinPoly()
	{
		// TODO
	}



	void FixSinPoly::emulate(TestCase* tc)
	{
		// get I/O values
		mpz_class svX = tc->getInputValue("X");
		mpz_class svR;
		int wOutFull = (msbIn<0 ? msbIn : 3*msbIn) - (lsbIn<0 ? 3*lsbIn : lsbIn) + 1 + 1;			//one more bit to cover all the range of produced bits

		if(truncated)
		{
			// compute the value of the result
			svR = svX*svX*svX;
			svR = svR/3;
			svR = (svX << (lsbIn-3*lsbIn + 1)) - svR;

			svR = (svR >> (wOutFull-(msbIn-lsbOut+1)));

			tc->addExpectedOutput("R", svR);

			svR += 1;
			svR &= ((1 << wOut) - 1);
		}
		else
		{
			// compute the value of the result
			svR = svX*svX*svX;
			svR = svR/3;
			svR = (svX << (lsbIn-3*lsbIn + 1)) - svR;
		}

		// add the result
		tc->addExpectedOutput("R", svR);
	}


	// Computes the weighted sum of the discarded bits if the full bit heap is truncated at weight k.
	int FixSinPoly::neededGuardBits(int k)
	{
		mpz_class sum, signBitSum, temp;
		int nbDigits, fullWOut, guardBits;

		//the size of the computations
		fullWOut = (msbIn<0 ? msbIn : 3*msbIn) - (lsbIn<0 ? 3*lsbIn : lsbIn) + 1 + 1;				//one more bit to cover all the range of produced bits

		REPORT(DEBUG, "fullWOut=" << fullWOut << " maximum weight allowed=" << fullWOut-1-(msbIn-k));

		//initialize the sum to 0
		sum = 0;

		//REPORT(DEBUG, "initially sum=" << sum);

		//add the truncated bits corresponding to sum_{i=imin}^imax(2^i*x_i)
		for(int i=lsbIn; i<k; i++)
			sum += (mpz_class(1) << (fullWOut-1-(msbIn-i)));

		//REPORT(DEBUG, "after adding bits from the initial number, sum=" << sum);

		//add the truncated terms corresponding to sum_{i=imin}^imax(2^(3i-1)*x_i)
		//	negated
		for(int i=0; ((i<=((msbIn-lsbIn+1)*3-2)-1) && (i<fullWOut-1-(msbIn-k))); i++)
		{
			sum += (mpz_class(1) << i);

			//REPORT(DEBUG, "added to sum " << (mpz_class(1) << i));

			for(int j=i; j<=fullWOut-1-(msbIn-k); j++)
			{
				signBitSum += (mpz_class(1) << j);

				//REPORT(DEBUG, "added to signBitSum " << (mpz_class(1) << j));
			}
		}

		//REPORT(DEBUG, "after adding bits from the first sum, sum=" << sum);

		//add the truncated terms corresponding to sum_i_j_imin^imax(2^(i+2j-1)*x_i*x_j)
		//	negated
		for(int i=lsbIn; i<msbIn; i++)
			for(int j=lsbIn; j<msbIn; j++)
			{
				//add the bit only if i != j
				if(i == j)
					continue;

				//add the bit only if it is less than 1 << fullWOut-1-(msbIn-k)
				if(((lsbIn<0 ? (i-lsbIn)+2*(j-lsbIn)-1 : i+2*j-1) + 1) > fullWOut-1-(msbIn-k))
					continue;

				sum += (mpz_class(1)<<((lsbIn<0 ? (i-lsbIn)+2*(j-lsbIn)-1 : i+2*j-1) + 1));

				//REPORT(DEBUG, "added to sum " << (mpz_class(1)<<((lsbIn<0 ? (i-lsbIn)+2*(j-lsbIn)-1 : i+2*j-1) + 1)));

				for(int l=(lsbIn<0 ? (i-lsbIn)+2*(j-lsbIn)-1 : i+2*j-1) + 1; l<=fullWOut-1-(msbIn-k); l++)
					signBitSum += (mpz_class(1)<<l);
			}

		//REPORT(DEBUG, "after adding bits from the second sum, sum=" << sum);

		//add the truncated terms corresponding to sum_i_j_k_imin^imax(2^(i+j+k)*x_i*x_j*x_k)
		//	negated
		for(int i=lsbIn; i<=msbIn; i++)
			for(int j=i+1; j<=msbIn; j++)
				for(int l=j+1; l<=msbIn; l++)
				{
					//add the bit only if it is less than 1 << fullWOut-1-(msbIn-k)
					if(((lsbIn<0 ? (i-lsbIn)+(j-lsbIn)+(l-lsbIn) : i+j+l) + 1) > fullWOut-1-(msbIn-k))
						continue;

					//i < j < k
					sum += (mpz_class(1)<<((lsbIn<0 ? (i-lsbIn)+(j-lsbIn)+(l-lsbIn) : i+j+l) + 1));

					//REPORT(DEBUG, "added to sum " << (mpz_class(1)<<((lsbIn<0 ? (i-lsbIn)+(j-lsbIn)+(l-lsbIn) : i+j+l) + 1)));

					for(int m=(lsbIn<0 ? (i-lsbIn)+(j-lsbIn)+(l-lsbIn) : i+j+l) + 1; m<=fullWOut-1-(msbIn-k); m++)
						signBitSum += (mpz_class(1)<<m);
				}

		//REPORT(DEBUG, "after adding bits from the third sum, sum=" << sum);

		//count the overflow bits
		//	count the total number of bits of the result
		signBitSum = signBitSum & ((mpz_class(1)<<(fullWOut-1-(msbIn-k)+1+1))-1);

		temp = sum + signBitSum;

		//REPORT(DEBUG, "temp=" << temp);

		if(temp == 0)
			nbDigits = 1;
		else
			nbDigits = 0;

		while(temp > 0)
		{
			//REPORT(DEBUG, "temp=" << temp);

			temp = (temp >> 1);
			nbDigits++;
		}

		//REPORT(DEBUG, "number of digits in the number: " << nbDigits);

		//	now compute the number of bits exceding the truncation limit k
		guardBits = nbDigits - (fullWOut-1-(msbIn-k));

		//REPORT(DEBUG, "number of guard bits: " << guardBits);

		//return the number of needed guard bits
		if(guardBits<0)
			return 0;
		else
			return guardBits;
	}

}



