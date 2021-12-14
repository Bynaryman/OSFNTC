#include <iostream>
#include <sstream>
#include "gmp.h"
#include "mpfr.h"

#include "sollya.h"

#include "FixSOPC.hpp"

#include "ConstMult/FixRealKCM.hpp"

using namespace std;
namespace flopoco{

	const int veryLargePrec = 6400;  /*6400 bits should be enough for anybody */

	FixSOPC::FixSOPC(OperatorPtr parentOp_, Target* target_, int lsbIn_, int lsbOut_, vector<string> coeff_) :
		Operator(parentOp_, target_),
		lsbOut(lsbOut_),
		coeff(coeff_),
		g(-1),
		computeMSBOut(true),
		computeGuardBits(true),
		addFinalRoundBit(true)
	{
		n = coeff.size();
		for (int i=0; i<n; i++) {
			msbIn.push_back(0);
			maxAbsX.push_back(1.0);
			lsbIn.push_back(lsbIn_);
		}
		initialize();
	}

	FixSOPC::FixSOPC(OperatorPtr parentOp_, Target* target_, int lsbIn_, int msbOut_, int lsbOut_, vector<string> coeff_) :
		Operator(parentOp_, target_),
		msbOut(msbOut_),
		lsbOut(lsbOut_),
		coeff(coeff_),
		g(-1),
		computeMSBOut(false),
		computeGuardBits(true),
		addFinalRoundBit(true)
	{
		n = coeff.size();
		for (int i=0; i<n; i++) {
			msbIn.push_back(0);
			maxAbsX.push_back(1.0);
			lsbIn.push_back(lsbIn_);
		}
		initialize();
	}


	FixSOPC::FixSOPC(OperatorPtr parentOp_, Target* target_, vector<int> msbIn_, vector<int> lsbIn_, int msbOut_, int lsbOut_, vector<string> coeff_, int g_, double targetError_) :
			Operator(parentOp_, target_),
			msbIn(msbIn_),
			lsbIn(lsbIn_),
			msbOut(msbOut_),
			lsbOut(lsbOut_),
			coeff(coeff_),
			g(g_),
			targetError(targetError_),
			computeMSBOut(false)
	{
		n = coeff.size();
		for(int i=0; i<n; i++)
			maxAbsX.push_back(intpow2(msbIn[i]));
		
		if (g==-1)
			computeGuardBits=true;
		
		addFinalRoundBit = (g==0 ? false : true);

		initialize();
	}


	FixSOPC::FixSOPC(OperatorPtr parentOp_, Target* target_, vector<double> maxAbsX_, vector<int> lsbIn_, int msbOut_, int lsbOut_, vector<string> coeff_, int g_, double targetError_) :
			Operator(parentOp_, target_),
			maxAbsX(maxAbsX_),
			lsbIn(lsbIn_),
			msbOut(msbOut_),
			lsbOut(lsbOut_),
			coeff(coeff_),
			g(g_),
			targetError(targetError_),
			computeMSBOut(false)
	{
		n = coeff.size();
		for(int i=0; i<n; i++)
			msbIn.push_back(ceil(log2(maxAbsX[i])));
		
		if (g==-1)
			computeGuardBits=true;

		addFinalRoundBit = (g==0 ? false : true);

		initialize();
	}


	FixSOPC::~FixSOPC()
	{
		for (int i=0; i<n; i++) {
			mpfr_clear(mpcoeff[i]);
		}
		// TODO destroy kcm[]
	}


	void FixSOPC::initialize()
	{
		srcFileName="FixSOPC";

		ostringstream name;
		name << "FixSOPC"; 
		setNameWithFreqAndUID(name.str()); 
	
		setCopyrightString("Matei Istoan, Louis Besème, Florent de Dinechin (2013-2015)");

		//reporting on the command line
		REPORT(DETAILED, "FixSOPC  lsbOut=" << lsbOut << "   g=" << g) ;
		for (int i=0; i< n; i++)
			REPORT(DETAILED, "i=" << i << "  coeff=" << coeff[i] << "  msbIn=" << msbIn[i] << "  lsbIn=" << lsbIn[i]);

		for (int i=0; i< n; i++)
			addInput(join("X",i), msbIn[i]-lsbIn[i]+1);
		
		for (int i=0; i< n; i++) {
			// parse the coeffs from the string, with Sollya parsing
			sollya_obj_t node;

			node = sollya_lib_parse_string(coeff[i].c_str());
			// If conversion did not succeed (i.e. parse error)
			if(node == 0)
				THROWERROR(srcFileName << ": Unable to parse string " << coeff[i] << " as a numeric constant");

			mpfr_init2(mpcoeff[i], veryLargePrec);
			sollya_lib_get_constant(mpcoeff[i], node);
			sollya_lib_clear_obj(node);
		}
		if(computeMSBOut)
		{
			mpfr_t sumAbsCoeff, absCoeff, mpMaxX;
			mpfr_init2 (sumAbsCoeff, veryLargePrec);
			mpfr_init2 (absCoeff, veryLargePrec);
			mpfr_init2 (mpMaxX, veryLargePrec);
			mpfr_set_d (sumAbsCoeff, 0.0, GMP_RNDN);

			for (int i=0; i< n; i++){
				// Accumulate the absolute values
				mpfr_abs(absCoeff,  mpcoeff[i], GMP_RNDU);
				mpfr_set_d(mpMaxX,  maxAbsX[i], GMP_RNDU);
				mpfr_mul(absCoeff,  absCoeff, mpMaxX, GMP_RNDU);
				mpfr_add(sumAbsCoeff,  sumAbsCoeff, absCoeff, GMP_RNDU);
			}

			// now sumAbsCoeff is the max value that the SOPC can take.
			double sumAbs = mpfr_get_d(sumAbsCoeff, GMP_RNDU); // just to make the following loop easier
			REPORT(DETAILED, "sumAbs=" << sumAbs);
			msbOut=1;
			while(sumAbs>=2.0){
				sumAbs*=0.5;
				msbOut++;
			}
			while(sumAbs<1.0){
				sumAbs*=2.0;
				msbOut--;
			}
			REPORT(INFO, "Computed msbOut=" << msbOut);
			mpfr_clears(sumAbsCoeff, absCoeff, mpMaxX, NULL);
		}
		else {
			REPORT(INFO, "Provided msbOut=" << msbOut);
		}

		addOutput("R", msbOut-lsbOut+1);

		int sumSize = 1 + msbOut - lsbOut ;
		REPORT(DETAILED, "Sum size is: "<< sumSize );


		// Now call all the KCM constructors for lsbOut, 
		//compute the guard bits and error for each, and deduce the overall guard bits.
		vector<FixRealKCM*> kcm;
		double targetUlpError = 1.0;
		double maxAbsError=0;
		for(int i=0; i<n; i++)		{
			REPORT(0, "i=" << i << "  msbIn[i]=" << msbIn[i] << "  lsbIn[i]=" << lsbIn[i]);
			// instantiating a KCM object. This call does not build any VHDL but computes errorInUlps out of the tentative architecture for g=0.
			FixRealKCM* m = new FixRealKCM(
																		 this,                         // the enveloping operator
																		 join("X",i), // input signal name
																		 true,        // input is signed
																		 msbIn[i],
																		 lsbIn[i],
																		 lsbOut,   // output LSB weight we want -- this is tentative
																		 coeff[i], // pass the string unmodified
																		 i==0,    //   the first KCM adds the round bit
																		 targetUlpError
																		 );
			kcm.push_back(m);
			REPORT(0, "KCM creation OK");
			double errorInUlps=m->getErrorInUlps();
			maxAbsError += errorInUlps;
			REPORT(DETAILED,"KCM for C" << i << "=" << coeff[i] << " entails an error of " <<  errorInUlps << " ulp(s)")
		}

		g = 0;
		double maxErrorWithGuardBits=maxAbsError;
		while (maxErrorWithGuardBits>(targetError>0 ? targetError : 0.5)) {
			g++;
			maxErrorWithGuardBits /= 2.0;
		}
		sumSize += g;
		REPORT(DETAILED,"Overall error is " << maxAbsError  << " ulps, which we will manage by adding " << g << " guard bits to the bit heap" );
		REPORT(DETAILED, "Sum size with KCM guard bits is: "<< sumSize << " bits.");
		
		if(!getTarget()->plainVHDL())
		{
			//create the bitheap that computes the sum
			bitHeap = new BitHeap(this, sumSize);

			// actually generate the code
			for(int i=0; i<n; i++)		{

				kcm[i]->addToBitHeap(bitHeap, g);
			}

			// The rounding bit is incorporated into the KCMs. 

			//compress the bitheap
			bitHeap -> startCompression();

			vhdl << tab << "R" << " <= " << bitHeap-> getSumName() << 
					range(sumSize-1, g) << ";" << endl;
		}
		else
		{
			THROWERROR("Sorry, plainVHDL doesn't work at the moment for FixSOPC. Somebody has to fix it and remove this message" );
			// Technically if you comment the line above it generates non-correct VHDL
#if 0 // FIXME port me to the new framework
			// All the KCMs in parallel
			for(int i=0; i< n; i++)	{
				FixRealKCM* mult = new FixRealKCM(this,
																					getTarget(), 
																					true, // signed
																					msbIn[i]-1, // input MSB,TODO one sign bit will be added by KCM because it has a non-standard interface. To be fixed someday
																					lsbIn[i], // input LSB weight
																					lsbOut-g, // output LSB weight -- the output MSB is computed out of the constant
																					coeff[i] // pass the string unmodified
																					);
				addSubComponent(mult);
				inPortMap(mult,"X", join("X", i));
				outPortMap(mult, "R", join("P", i));
				vhdl << instance(mult, join("mult", i));
			}
			// Now build a pipelined rake
			vhdl << tab << declare("S0", sumSize) << " <= " << zg(sumSize) << ";" << endl;
			for(int i=0; i< n; i++)		{
				// Addition
				int pSize = getSignalByName(join("P", i))->width();
				vhdl << tab << declare(getTarget()->adderDelay(sumSize),
															 join("S", i+1), sumSize)
						 << " <= " <<  join("S",i);
				vhdl << " + (" ;
				if(sumSize>pSize)
					vhdl << "("<< sumSize-1 << " downto " << pSize<< " => "<< join("P",i) << of(pSize-1) << ")" << " & " ;
				vhdl << join("P", i) << ");" << endl;
			}

			// Rounding costs one more adder to add the half-ulp round bit.
			// This could be avoided by pushing this bit in one of the KCM tables
			vhdl << tab << declare(getTarget()->adderDelay(sumSize+1),
														 "R_int", sumSize+1)
					 << " <= " <<  join("S", n) << range(sumSize-1, sumSize-(msbOut-lsbOut+1)-1) << " + (" << zg(sumSize) << " & \'1\');" << endl;
			vhdl << tab << "R <= " <<  "R_int" << range(sumSize, 1) << ";" << endl;
#endif
		} // end plain vhdl
	 

	};





	// Function that factors the work done by emulate() of FixFIR and the emulate() of FixSOPC
	pair<mpz_class,mpz_class> FixSOPC::computeSOPCForEmulate(vector<mpz_class> inputs) {
		// Not completely safe: we compute everything on veryLargePrec, and hope that rounding this result is equivalent to rounding the exact result
		mpfr_t t, s, rd, ru;
		mpfr_init2 (t, veryLargePrec);
		mpfr_init2 (s, veryLargePrec);
		mpfr_set_d(s, 0.0, GMP_RNDN); // initialize s to 0
		for (int i=0; i< n; i++)	{
			mpfr_t x;
			mpz_class sx = bitVectorToSigned(inputs[i], 1+msbIn[i]-lsbIn[i]); 						// convert it to a signed mpz_class
			mpfr_init2 (x, 1+msbIn[i]-lsbIn[i]);
			mpfr_set_z (x, sx.get_mpz_t(), GMP_RNDD); 				// convert this integer to an MPFR; this rounding is exact
			mpfr_mul_2si (x, x, lsbIn[i], GMP_RNDD); 						// multiply this integer by 2^-lsb to obtain a fixed-point value; this rounding is again exact

			mpfr_mul(t, x, mpcoeff[i], GMP_RNDN); 					// Here rounding possible, but precision used is ridiculously high so it won't matter
			mpfr_add(s, s, t, GMP_RNDN); 							// same comment as above
			mpfr_clears (x, NULL);
		}
		// now we should have in s the (very accurate) sum
		// round it up and down
#if 0
		if(mpfr_get_d(s, GMP_RNDN)>=0)
			cerr << "+";
		else
			cerr << "-";
#endif
		
		// make s an integer -- no rounding here
		mpfr_mul_2si (s, s, -lsbOut, GMP_RNDN);

		mpfr_init2 (rd, 1+msbOut-lsbOut);
		mpfr_init2 (ru, 1+msbOut-lsbOut);

		mpz_class rdz, ruz;

		mpfr_get_z (rdz.get_mpz_t(), s, GMP_RNDD); 					// there can be a real rounding here
		rdz=signedToBitVector(rdz, 1+msbOut-lsbOut);

		mpfr_get_z (ruz.get_mpz_t(), s, GMP_RNDU); 					// there can be a real rounding here
		ruz=signedToBitVector(ruz, 1+msbOut-lsbOut);

		mpfr_clears (t, s, rd, ru, NULL);

		return make_pair(rdz, ruz);
	}





	void FixSOPC::emulate(TestCase * tc) {
		vector<mpz_class> inputs;
		for (int i=0; i< n; i++)	{
			mpz_class sx = tc->getInputValue(join("X", i)); 		// get the input bit vector as an integer
			inputs.push_back(sx);
		}
		pair<mpz_class,mpz_class> results = computeSOPCForEmulate(inputs);

		tc->addExpectedOutput ("R", results.first);
		tc->addExpectedOutput ("R", results.second);
	}

	OperatorPtr FixSOPC::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int lsbIn;
		UserInterface::parseInt(args, "lsbIn", &lsbIn);
		int lsbOut;
		UserInterface::parseInt(args, "lsbOut", &lsbOut);

		vector<string> coeffs;
		UserInterface::parseColonSeparatedStringList( args, "coeff", &coeffs);
		
		return new FixSOPC(parentOp, target, lsbIn, lsbOut, coeffs);
	}


	OperatorPtr FixSOPC::parseArgumentsFull(OperatorPtr parentOp, Target *target, vector<string> &args) {

		vector<int> msbIn;
		UserInterface::parseColonSeparatedIntList(args, "msbIn", &msbIn);

		vector<int> lsbIn;
		UserInterface::parseColonSeparatedIntList(args, "lsbIn", &lsbIn);

		int msbOut;
		UserInterface::parseInt(args, "msbOut", &msbOut);

		int lsbOut;
		UserInterface::parseInt(args, "lsbOut", &lsbOut);

		vector<string> coeffs;
		UserInterface::parseColonSeparatedStringList( args, "coeff", &coeffs);
		
		return new FixSOPC(parentOp, target, msbIn, lsbIn, msbOut, lsbOut, coeffs);
	}




	
	void FixSOPC::registerFactory(){
		UserInterface::add("FixSOPC", // name
											 "A fix-point Sum of Product by Constants.",
											 "FiltersEtc", // categories
											 "",
											 "lsbIn(int): input's last significant bit;\
                        lsbOut(int): output's last significant bit;\
                        coeff(string): colon-separated list of real coefficients using Sollya syntax. Example: coeff=\"1.234567890123:sin(3*pi/8)\"",
											 "",
											 FixSOPC::parseArguments,
											 FixSOPC::unitTest
											 ) ;
		UserInterface::add("FixSOPCfull", // name
											 "A fix-point Sum of Product by Constants (detailed interface).",
											 "FiltersEtc", // categories
											 "",
											 "msbIn(string): colon-separated string of ints, input's last significant bit;\
                        lsbIn(string): colon-separated string of ints, input's last significant bit;\
                        msbOut(int): output's most significant bit;\
                        lsbOut(int): output's last significant bit;\
                        coeff(string): colon-separated list of real coefficients using Sollya syntax. Example: coeff=\"1.234567890123:sin(3*pi/8)\"",
											 "",
											 FixSOPC::parseArgumentsFull
											 ) ;
	}


	// please fill me with regression tests or corner case tests
	void FixSOPC::buildStandardTestCases(TestCaseList * tcl) {
		// TODO add the inputs that entail the max of the outputs 
#if 0
		TestCase *tc;
		// first few cases to check emulate()
		// All zeroes
		tc = new TestCase(this);
		for(int i=0; i<n; i++)
			tc->addInput(join("X",i), mpz_class(0) );
		emulate(tc);
		tcl->add(tc);

		// All ones (0.11111)
		tc = new TestCase(this);
		for(int i=0; i<n; i++)
			tc->addInput(join("X",i), (mpz_class(1)<<p)-1 );
		emulate(tc);
		tcl->add(tc);

		// n cases with one 0.5 and all the other 0s
		for(int i=0; i<n; i++){
			tc = new TestCase(this);
			for(int j=0; j<n; j++){
				if(i==j)
					tc->addInput(join("X",j), (mpz_class(1)<<(p-1)) );
				else
					tc->addInput(join("X",j), mpz_class(0) );
			}
			emulate(tc);
			tcl->add(tc);
		}
#endif
	}

	
	TestList FixSOPC::unitTest(int index)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;
		
		if(index==-1) 	{ // The unit tests
			for(int n=3; n<9; n+=2){

				// build stupid coeff list with positive and negative numbers. I don't think this corresponds to an actual filter.
				ostringstream c1,c2;
				c1 << "\"";
				c2 << "\"";
				for (int i=1; i<n; i++) {
					c1 << "sin(pi*" << i << "/" << n << ")"; // only positive numbers
					c2 << "cos(pi*" << i << "/" << n << ")"; // pos and neg numbers
					if(i<n-1) {
						c1 << ":";
						c2 << ":";
					}
				}
				c1 << "\"";
				c2 << "\"";
				
				for(int lsb=-3; lsb>-16; lsb--) { // test various input widths
					paramList.clear();
					paramList.push_back(make_pair("lsbIn",  to_string(lsb)));
					paramList.push_back(make_pair("lsbOut", to_string(lsb))); // same LSB, this tests enough
					paramList.push_back(make_pair("coeff",  c1.str() ));
					paramList.push_back(make_pair("TestBench n=",  "1000"));
					testStateList.push_back(paramList);

					paramList.clear();
					paramList.push_back(make_pair("lsbIn",  to_string(lsb)));
					paramList.push_back(make_pair("lsbOut", to_string(lsb))); // same LSB, this tests enough
					paramList.push_back(make_pair("coeff",  c2.str() ));
					paramList.push_back(make_pair("TestBench n=",  "1000"));
					testStateList.push_back(paramList);
				}
			}
		}
		else     
		{
				// finite number of random test computed out of index
		}	

		return testStateList;
	}

	
}
