// TODO: repair FixFIR, FixIIR, FixComplexKCM
/*
 * A faithful multiplier by a real constant, using a variation of the KCM method
 *
 * This file is part of the FloPoCo project developed by the Arenaire
 * team at Ecole Normale Superieure de Lyon
 * 
 * Authors : Florent de Dinechin, Florent.de.Dinechin@ens-lyon.fr
 * 			 3IF-Dev-Team-2015
 *
 * Initial software.
 * Copyright © ENS-Lyon, INRIA, CNRS, UCBL, 
 * 2008-2011.
 * All rights reserved.
 */
/*
% TODO: implement the following (from the Good Book)
    \item if $C_i$ is an integer multiple of $2^{\ell_R-\ell_i}$, as $Xi_i$ is either $0$, or an integer multiple of  $2^{\ell_i}$ then for $X_i>0$, $|C_iX_i|>2^{\ell_R}$:
      in other words the multiplication produces no bit to the right of $2^{\ell_R}$.
      Therefore,  whatever $g\ge 0$, no rounding will be needed, hence  $\bound{\abserr^{\text{(ulp)}}_i}=0$ (since the case $X_i=0$, of course, also entails $\abserr^{\text{(ulp)}}_i=0$).

TODO: IntIntKCM is a special case of FixReal.
This is the good way of getting rid of IntIntKCM.

Experiment:  fpc FixRealKCM frequency=100 signedinput=1 msbin=23 lsbin=0 lsbout=0 constant=13176795 
We have the good operator interface.
The tables are full of zeroes, including a few gard bits that are useless in this case and a round bit.
Therefore the bit heap is larger than should be.
Still, we get 188 LUTs where Walters report that Logicore does it in 171
The delay is quite bad (8ns).

Instead of resurrecting IntIntKCM, let's do the following
- compute lsbC such that C=I*2^lsbC and I is an integer
(this lsbC doesn't always exist, e.g. if we input 1/3, so we must place a reasonable limit)
For an odd integer we will get lsbC=0.
- compute m_c = intlog2(I)
- If lsbOut <= lsbC + lsbIn then we have an exact operation.
 We can actually directly output zeroes for the (lsbC+lsbIn) - lsbOut bits.
- For each table, check if it is exact (it will be in the case of an exact operation)
 and avoid adding zero bits to the bit heap.

Then we have a working IntIntKCM, that even does useful optimizations for all sorts of constants.

*/



#include "../Operator.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include <sollya.h>
#include "../utils.hpp"
#include "FixRealKCM.hpp"
#include "../IntAddSubCmp/IntAdder.hpp"

using namespace std;


namespace flopoco{


	//standalone operator
	FixRealKCM::FixRealKCM(OperatorPtr parentOp, Target* target, bool signedIn_, int msbIn_, int lsbIn_, int lsbOut_, string constant_, double targetUlpError_):
		FixRealConstMult(parentOp, target, signedIn_, msbIn_, lsbIn_, lsbOut_, constant_, targetUlpError_),
		addRoundBit(true)
	{
		thisOp = this;

		vhdl << "-- This operator multiplies by "<<constant << endl;
		init();		 // check special cases, computes number of tables and errorInUlps.

		// Now we have everything to compute g
		computeGuardBits();
		
		// To help debug KCM called from other operators, report in FloPoCo CLI syntax
		REPORT(DETAILED, "FixRealKCM  signedIn=" << signedIn << " msbIn=" << msbIn << " lsbIn=" << lsbIn << " lsbOut=" << lsbOut << " constant=\"" << constant << "\"  targetUlpError="<< targetUlpError);
		
		addInput("X",  msbIn-lsbIn+1);
		//		addFixInput("X", signedIn,  msbIn, lsbIn); // The world is not ready yet
		inputSignalName = "X"; // for buildForBitHeap
		addOutput("R", msbOut-lsbOut+1);

		// Special cases
		if(constantRoundsToZeroInTheStandaloneCase || constantIsExactlyZero)	{
			vhdl << tab << "R" << " <= " << zg(msbOut-lsbOut+1) << ";" << endl;
			return;
		}

		if(constantIsPowerOfTwo)	{
			// The code here is different that the one for the bit heap constructor:
			// In the stand alone case we must compute full negation.
			string rTempName = createShiftedPowerOfTwo(inputSignalName); 
			int rTempSize = thisOp->getSignalByName(rTempName)->width();

			if(negativeConstant) { // In this case msbOut was incremented in init()
				vhdl << tab << "R" << " <= " << zg(msbOut-lsbOut+1) << " - ";
				if(signedIn) {
					vhdl << "("
							 <<  rTempName << of(rTempSize-1) << " & " // sign extension 
							 <<  rTempName << range(rTempSize-1, g)
							 << ");" << endl;
				}
				else{ // unsigned input
					vhdl <<  rTempName << range(rTempSize-1, g) << ";" << endl;
				}
			}
			else{		
				vhdl << tab << "R <= "<< rTempName << range(msbOut-lsbOut+g, g) << ";" << endl;
			}
			return;
		}


		// From now we have stuff to do.
		//create the bitheap
		//		int bitheaplsb = lsbOut - g;
		REPORT(DEBUG, "Creating bit heap for msbOut=" << msbOut <<" lsbOut=" << lsbOut <<" g=" << g);
		bitHeap = new BitHeap(this, msbOut-lsbOut+1+g); // hopefully some day we get a fixed-point bit heap
		
		buildTablesForBitHeap(); // does everything up to bit heap compression

		//compress the bitheap and produce the result
		bitHeap->startCompression();

		// Retrieve the bits we want from the bit heap
		vhdl << tab << declare("OutRes",msbOut-lsbOut+1+g) << " <= " << 
			bitHeap->getSumName() << range(msbOut-lsbOut+g, 0) << ";" << endl; // This range is useful in case there was an overflow?

		vhdl << tab << "R <= OutRes" << range(msbOut-lsbOut+g, g) << ";" << endl;
	}
	


	
	//Constructor for the virtual KCM.
	FixRealKCM::FixRealKCM(
												 Operator* thisOp_, // the operator this virtual op belongs to 
												 string multiplicandX,
												 bool signedIn_,
												 int msbIn_,
												 int lsbIn_,
												 int lsbOut_, 
												 string constant_,
												 bool addRoundBit_,
												 double targetUlpError_
												 ):
		FixRealConstMult(thisOp_->getParentOp(), thisOp_->getTarget(), signedIn_, msbIn_, lsbIn_, lsbOut_, constant_, targetUlpError_),
		addRoundBit(addRoundBit_), 
		bitHeap(NULL), // will be set by buildForBitHeap()
		inputSignalName(multiplicandX),
		thisOp(thisOp_)
	{

		init(); // check special cases, computes number of tables, but do not compute g: just take lsbOut as it is.
		
	}
					







	/**
	* @brief init : all operator initialization stuff goes here
	*/
	// Init computes the table splitting, because it is independent of g.
	// It may take suboptimal decisions if lsbOut is later enlarged with guard bits. 
	void FixRealKCM::init()
	{
		//useNumericStd();

		srcFileName="FixRealKCM";

		setCopyrightString("Florent de Dinechin (2007-2016)");

		sollya_lib_set_roundingwarnings(sollya_lib_parse_string("off"));

		if(lsbIn>msbIn) 
			THROWERROR("FixRealKCM: Error, lsbIn=" << lsbIn<< " > msbIn="<<msbIn);
    
		if(targetUlpError > 1.0)
			THROWERROR("FixRealKCM: Error, targetUlpError="<<
					targetUlpError<<">1.0. Should be in ]0.5 ; 1].");
		//Error on final rounding is er <= 2^{lsbout - 1} = 1/2 ulp so 
		//it's impossible to reach 0.5 ulp of precision if cste is real
		if(targetUlpError <= 0.5) 
			THROWERROR("FixRealKCM: Error, targetUlpError="<<
					targetUlpError<<"<0.5. Should be in ]0.5 ; 1]");

		constStringToSollya();

		// build the name
		ostringstream name; 
#if 0 // Do we really need all this noise when we don't even have the constant? We have uid anyway
		name <<"FixRealKCM_"  << vhdlize(msbIn)
				 << "_"  << vhdlize(lsbIn)
				 <<	"_" << vhdlize(lsbOut)
				 << "_" << vhdlize(constant)
				 << (signedIn  ?"_signed" : "_unsigned");
#else
		name <<"FixRealKCM";
#endif
		
		setNameWithFreqAndUID(name.str());

		// Now we can check when this is a multiplier by 0: either because the it is zero, or because it is close enough
		constantIsExactlyZero = false;
		constantRoundsToZeroInTheStandaloneCase = false;
		if(mpfr_zero_p(mpC) != 0){
			constantIsExactlyZero = true;
			msbOut=lsbOut; // let us return a result on one bit, why not.
			errorInUlps=0;
			REPORT(INFO, "It seems somebody asked for a multiplication by 0. We can do that.");
			return;
		}
		
		// A few sanity checks related to the magnitude of the constant
		
		msbOut =   msbIn + msbC;
		
		// A bit of weight l is sent to position l+msbC+1 at most.
		// msbIn is sent to msbIn+ msbC +1 at most
		if(msbOut<lsbOut){
			constantRoundsToZeroInTheStandaloneCase = true;
			REPORT(INFO, "If nobody adds guard bits, multiplying the input by such a small constant will always returns 0. This could simplify the architecture.");
			msbOut=lsbOut; // let us return a result on one bit, why not.
			errorInUlps=0.5;// TODO this is an overestimation
			return;
		}
		
		// Now even if the constant doesn't round completely to zero, it could be small enough that some of the inputs bits will have little impact on the output
		// However this depends on how many guard bits we add...
		// So the relevant test has been pushed to the table generation

		// Finally, check if the constant is a power of two -- obvious optimization there
		constantIsPowerOfTwo = (mpfr_cmp_ui_2exp(absC, 1, msbC) == 0);
		if(constantIsPowerOfTwo) {
			if(negativeConstant) {
				msbOut++; // To cater for the asymmetry of fixed-point : -2^(msbIn-1) has no representable opposite otherwise
				// We still have the original shift information in msbC
			}
			
		
			if(lsbIn+msbC<lsbOut) {   // The truncation case
				REPORT(DETAILED, "Constant is a power of two. Simple shift will be used instead of tables, but still there will be a truncation");
				errorInUlps=1;
			}

			
			else { // The padding case
				// The stand alone constructor computes a full subtraction. The Bitheap one adds negated bits, and a constant one that completes the subtraction.
				REPORT(DETAILED, "Constant is a power of two. Simple shift will be used instead of tables, and this KCM will be exact");
				errorInUlps=0;
			}
			return; // init stops here.
		}
		
		REPORT(DETAILED, "msbConstant=" << msbC  << "   (msbIn,lsbIn)=("<< 
					 msbIn << "," << lsbIn << ")    lsbIn=" << lsbIn << 
				"   (msbOut,lsbOut)=(" << msbOut << "," << lsbOut <<
					 ")  signedOutput=" << signedOutput
		);

		// Compute the splitting of the input bits.
		// Ordering: Table 0 is the one that inputs the MSBs of the input 
		// Let us call l the native LUT size (discounting the MUXes internal to logic clusters that enable to assemble them as larger LUTs).
		// Asymptotically (for large wIn), it is better to split the input in l-bit slices than (l+1)-bit slices, even if we have these muxes for free:
		// costs (in LUTs) are roundUp(wIn/l) versus 2*roundUp(wIn/(l+1))
		// Exception: when one of the tables is a 1-bit one, having it as a separate table will cost one LUT and one addition more,
		// whereas integrating it in the previous table costs one more LUT and one more MUX (the latter being for free) 
		// Fact: the tables which inputs MSBs have larger outputs, hence cost more.
		// Therefore, the table that will have this double LUT cost should be the rightmost one.

		
		int optimalTableInputWidth = getTarget()->lutInputs();
		//		int wIn = msbIn-lsbIn+1;

		// first do something naive, then we'll optimize it.
		numberOfTables=0;
		int tableInMSB= msbIn; 
		int tableInLSB = msbIn-(optimalTableInputWidth-1);
		while (tableInLSB>lsbIn) {
			m.push_back(tableInMSB);
			l.push_back(tableInLSB);
			tableInMSB -= optimalTableInputWidth ;
			tableInLSB -= optimalTableInputWidth ;
			numberOfTables++;
		}
		tableInLSB=lsbIn;

		// For the last table we just check if is not of size 1
		if(tableInLSB==tableInMSB && numberOfTables>0) {
			// better enlarge the previous table
			l[numberOfTables-1] --;
		}
		else { // last table is normal
			m.push_back(tableInMSB);
			l.push_back(tableInLSB);
			numberOfTables++;			
		}
		
		// Are the table outputs signed? We compute this information here in order to avoid having constant bits added to the bit heap.
		// The chunk input to tables of index i>0 is always an unsigned number, so their output will be constant, fixed by the sign of 
		for (int i=0; i<numberOfTables; i++) {
			int s;
			if (i==0 && signedIn) {
				// This is the only case when we can have a variable sign
					s=0;
			}
			else { // chunk input is positive
					if(negativeConstant)
						s=-1;
					else
						s=1;
			}
			tableOutputSign.push_back(s);
		}
		/* How to use this information? 
			 The case +1 is simple: don't tabulate the constant 0 sign, don't add it to the bit heap 
			 The case 0 is usual: we have a two's complement number to add to the bit heap, 
          there are methods for that.  
       The case -1 presents an additional problem: the sign bit is not always 1, 
			    indeed there is one entry were the sign bit is 0: when input is 0.
					As usual there is a bit heap trick (found by Luc Forget): 
          negate all the bits and add one ulp to the constant vector of the bit heap.
					
					For simplicity, in the current code we compute xi*absC (the abs. value of the constant)
          then subtract this from the bitHeap.
					The effect is exactly the same (hoping that the not gets merged in the table) 
					and the C++ code is simpler.
*/
		for (int i=0; i<numberOfTables; i++) {
			REPORT(DETAILED, "Table " << i << "   inMSB=" << m[i] << "   inLSB=" << l[i] << "   tableOutputSign=" << tableOutputSign[i]  );
		}

		// Finally computing the error due to this setup. We express it as ulps at position lsbOut-g, whatever g will be
		errorInUlps=0.5*numberOfTables;
		REPORT(DETAILED,"errorInUlps=" << errorInUlps);
	}




		

	double FixRealKCM::getErrorInUlps() {
		return errorInUlps;
	}


	
	void FixRealKCM::computeGuardBits(){
		if(numberOfTables==2 && targetUlpError==1.0)
			g=0; // specific case: two CR tables make up a faithful sum
		else{
			// Was:			g = ceil(log2(numberOfTables/((targetUlpError-0.5)*exp2(-lsbOut)))) -1 -lsbOut;
			g=0;
			double maxErrorWithGuardBits=errorInUlps;
			double tableErrorBudget = targetUlpError-0.5 ; // 0.5 is for the final rounding
			while (maxErrorWithGuardBits > tableErrorBudget) {
				g++;
				maxErrorWithGuardBits /= 2.0;
			}
		}
		REPORT(DETAILED, "For errorInUlps=" << errorInUlps << " and targetUlpError=" << targetUlpError << "  we compute g=" << g);
	}


	void FixRealKCM::addToBitHeap(BitHeap* bitHeap_, int g_) {
		bitHeap=bitHeap_;
		g=g_;

		bool special = specialCasesForBitHeap();

		if(!special)
			buildTablesForBitHeap();
	}


	bool FixRealKCM::specialCasesForBitHeap() {
	// Special cases
		if(constantIsExactlyZero)		{
			return true; 			// do nothing
		}
		// We don't consider constantRoundsToZeroInTheStandaloneCase: 
		// maybe init() evaluated that the constant rounds to zero, but init() didn't know
		// how many gard bits will be needed (and at this point of the execution we don't know yet, either).
		// For enough guard bits, the constant won't round to zero anymore.
		// An instance of this problem is a large rescaling FixFIR, which has only small constants but adds many guard bits...
		// And this has been a bug for quite a while...


		// In all the following it should be clear that lsbOut is the initial lsb asked by the larger operator.
		// Some g was computed and we actually compute/tabulate to lsbOut -g
		// We keep these two variables separated because it makes this two-step process more explicit than changing lsbOut.
		
		if(constantIsPowerOfTwo)	{
			// Create a signal that shifts the input (so multiplies by the _abs val_),
			// or truncates it into place
			string rTempName = createShiftedPowerOfTwo(inputSignalName);
			//Signal* rTemp = thisOp->getSignalByName(rTempName);

			if(negativeConstant) {
				bitHeap -> subtractSignal(rTempName);
			}
			else{ // positive constant
				bitHeap -> addSignal(rTempName);
			}
			return true; // and that 's all folks.
			}
		// From now on we have a "normal" constant and we instantiate tables.
		return false;
	}

	
	// Just to factor out code.
	/* This builds the input shifted WRT lsb-g
	 but doesn't worry about adding or subtracting it,
	 which depends wether we do a standalone or bit-heap   
	*/
	string FixRealKCM::createShiftedPowerOfTwo(string resultSignalName){
		string rTempName = getName() + "_Rtemp"; // Should be unique in a bit heap if each KCM got a UID.
		// Compute shift that must be applied to x to align it to lsbout-g.
		// This is a shift left: negative means shift right.
		int shift= lsbIn -(lsbOut-g)  + msbC ; 
		int rTempSize = msbC+msbIn -(lsbOut -g) +1; // initial msbOut is msbC+msbIn
		REPORT(DETAILED,"Power of two, msbC=" << msbC << "     Shift left of " << shift << " bits");
		// compute the product by the abs constant

		thisOp->vhdl << tab << thisOp->declare(rTempName, rTempSize) << " <= ";

		// Still there are two cases:
		if(shift>=0) {  // Shift left; pad   THIS SEEMS TO WORK
			thisOp->vhdl << inputSignalName << " & " << zg(shift);
			// rtempsize= msbIn-lsbin+1   + shift  =   -lsbIn   + msbIn   +1   - (lsbOut-g -lsbIn +msbC)
		}
		else { // shift right; truncate
			thisOp->vhdl << inputSignalName << range(msbIn-lsbIn, -shift);
		}
#if 1 // This used to break the lexer, I keep it as a case study to fix it. TODO
		thisOp->vhdl <<  "; -- constant is a power of two, shift left of " << shift << " bits" << endl;
#else
		ostringstream t;
		t << "; -- constant is a power of two, shift left of " << shift << " bits" << endl;
		thisOp->vhdl <<  t.str();
#endif
		// copy the signedness into rtemp
		thisOp->getSignalByName(rTempName)->setIsSigned(signedIn);
		return rTempName;
	}


	void FixRealKCM::buildTablesForBitHeap() {

		for(int i=0; i<numberOfTables; i++) {
			string sliceInName = join(getName() + "_A", i); // Should be unique in a bit heap if each KCM got a UID.
			string sliceOutName = join(getName() + "_T", i); // Should be unique in a bit heap if each KCM got a UID.
			string instanceName = join(getName() + "_Table", i); // Should be unique in a bit heap if each KCM got a UID.

			// Now that we have g we may compute if it has useful output bits
			int tableOutSize = m[i] + msbC  - lsbOut + g +1; // TODO: the +1 could sometimes be removed
			if(tableOutSize<=0) {  
				REPORT(DETAILED, " *** Table " << i << " was contributing nothing to the bit heap and has been discarded");
				// The remaining autotest last-bit bug is here.	
			}
			else { // Build it and add its output to the bit heap
				// REPORT(DEBUG, "lsbIn=" << lsbIn);
				thisOp->vhdl << tab << thisOp->declare(sliceInName, m[i]- l[i] +1 ) << " <= "
											 << inputSignalName << range(m[i]-lsbIn, l[i]-lsbIn) << ";"
											 << "-- input address  m=" << m[i] << "  l=" << l[i]
											 << endl;

				vector<mpz_class> tableContent = kcmTableContent(i);

				string tablename;
				if(thisOp==this) // non-virtual
					tablename = join(getName()+"_T",i);
				else
					tablename = join(thisOp->getName()+"_"+getName()+"_T",i);
					
				schedule();

				thisOp->inPortMap ("X", sliceInName);
				thisOp->outPortMap("Y", sliceOutName);

				Table* t = new Table(thisOp->getParentOp(),
														 thisOp->getTarget(),
														 tableContent,
														 tablename, //name
														 m[i] - l[i]+1, // wIn
														 tableOutSize, //wOut
														 1 // logicTable
														 );
				
				thisOp->vhdl << thisOp->instance(t , instanceName);

				Signal* sliceOut = thisOp->getSignalByName(sliceOutName);

				// int sliceOutWidth = sliceOut->width();
				//cerr << "***** Size of "<< sliceOutName << " is " << sliceOutWidth	 << " " << tableOutputSign[i] << endl;
				// Add these bits to the bit heap
				switch(tableOutputSign[i]) {
				case 0:
					//					cout << "****************************Case 0" << endl;
					// bitHeap -> addSignedBitVector(
					// 															sliceOutName, // name
					// 															sliceOutWidth, // size
					// 															0 // weight
					// 															);
					sliceOut -> setIsSigned();
					bitHeap -> addSignal(sliceOutName);
					break;
				case 1:
					//					cout << "****************************Case 1" << endl;
					sliceOut -> setIsSigned(false);
					bitHeap -> addSignal(sliceOutName);
					break;
				case -1: // In this case the table simply stores x* absC 
					//					cout << "****************************Case -1" << endl;
					sliceOut -> setIsSigned(false);
					bitHeap -> subtractSignal(sliceOutName);
					break;
				default: THROWERROR("unexpected value in tableOutputSign");
				}
			}
		}
	}			


	// TODO: get rid of this in favor of FixRealConstMult, but requires a bit of refactoring first:
	// it is used at least in FixSOPC and FPExp
	
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
				"This variant of Ken Chapman's Multiplier is briefly described in <a href=\"bib/flopoco.html#volkova:hal-01561052\">this article</a>.<br> Special constants, such as 0 or powers of two, are handled efficiently.",
				FixRealKCM::parseArguments,
				FixRealKCM::unitTest
		);
	}

	
	/************************** The FixRealKCMTable class ********************/

	
	vector<mpz_class> FixRealKCM::kcmTableContent(int i) {
		vector<mpz_class> r;
		int wIn = m[i] - l[i]+1;
		int wOut = 	m[i] + msbC  - lsbOut + g +1;
		REPORT(DEBUG, "Table "<<i<<" wOut=" << wOut);
		for (int x0=0; x0 < (1<<wIn); x0++) {
			// get rid of two's complement
			int x=x0;
			//Only the MSB "digit" has a negative weight
			if(tableOutputSign[i]==0)	{ // only in this case interpret input as two's complement
				if ( x > ((1<<(wIn-1))-1) )	{
					x -= (1<<wIn);
				}
			} // Now x is a signed number only if it was chunk 0 and its sign bit was set

			//cout << "i=" << i << " x0=" << x0 << "  sx=" << x <<"  wIn="<<wIn<< "   "  <<"  wout="<<wOut<< "   " ;

			mpz_class result;
			mpfr_t mpR, mpX;

			mpfr_init2(mpR, 10000);
			mpfr_init2(mpX, 2*wIn); //To avoid mpfr bug if wIn = 1
		                       
			mpfr_set_si(mpX, x, GMP_RNDN); // should be exact
			// Scaling so that the input has its actual weight
			mpfr_mul_2si(mpX, mpX, l[i], GMP_RNDN); //Exact

			//						double dx = mpfr_get_d(mpX, GMP_RNDN);
			//			cout << "input as double=" <<dx << "  l[i]="  << l[i] << "    ";
			
			// do the mult in large precision
			if(tableOutputSign[i]==0)	
				mpfr_mul(mpR, mpX, mpC, GMP_RNDN);
			else // multiply by the absolute value of the constant, the bitheap logic does the rest
				mpfr_mul(mpR, mpX, absC, GMP_RNDN);
			
			// Result is integer*mpC, which is more or less what we need: just scale it to an integer.
			mpfr_mul_2si( mpR, 
										mpR,
										-lsbOut + g,	
										GMP_RNDN	); //Exact

			//			double dr=mpfr_get_d(mpR, GMP_RNDN);
			//			cout << "  dr=" << dr << "  ";
			
			// Here is when we do the rounding
			mpfr_get_z(result.get_mpz_t(), mpR, GMP_RNDN); // Should be exact

			//cout << tableOutputSign[i] << "  result0=" << result << "  ";

		
			//cout  << result << "  " <<endl;

			// Add the rounding bit to the table 0 if there are guard bits,
			if(addRoundBit && (i==0) && (g>0)) {
				int roundBit=1<<(g-1);
				// but beware: the table output may be subtracted (see buildTablesForBitHeap() )
				if(tableOutputSign[0] >= 0) // This table will be added
					result += roundBit;
				else // This table will be subtracted
					result -= roundBit;
			}
						// sign management
			if(result<0) {
				if(tableOutputSign[i]!=1) {
					// this is a two's complement number with a non-constant sign bit
					// so we encode it as two's complement
					result +=(mpz_class(1) << wOut);				
				}
				else{
					THROWERROR("kcmTableContent: For table " << i << " at index " << x0 << ", the computed table entry is " << result <<". This table should contain positive values");
				}
			}
			r.push_back(result);
		}
		return r;
	}


	
}





