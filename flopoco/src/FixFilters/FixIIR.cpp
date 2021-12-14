#include <iostream>
#include <sstream>
#include <iomanip>

#include "gmp.h"
#include "mpfr.h"
#if HAVE_WCPG
extern "C"
{
	#include "wcpg.h"
}
#endif

#include "FixIIR.hpp"

#include "sollya.h"

#include "ShiftReg.hpp"
#include "FixSOPC.hpp"


/* Test with 
./flopoco FixIIR coeffb="1:2" coeffa="1/2:1/4" lsbIn=-12 lsbOut=-12   TestBench n=1000


The example with poles close to 1:
./flopoco generateFigures=1 FixIIR coeffb="0x1.89ff611d6f472p-13:-0x1.2778afe6e1ac0p-11:0x1.89f1af73859fap-12:0x1.89f1af73859fap-12:-0x1.2778afe6e1ac0p-11:0x1.89ff611d6f472p-13" coeffa="-0x1.3f4f52485fe49p+2:0x1.3e9f8e35c8ca8p+3:-0x1.3df0b27610157p+3:0x1.3d42bdb9d2329p+2:-0x1.fa89178710a2bp-1" lsbIn=-12 lsbOut=-12 TestBench n=10000
Remarque: H prend du temps à calculer sur cet exemple.

A small Butterworth
 ./flopoco generateFigures=1 FixIIR coeffb="0x1.7bdf4656ab602p-9:0x1.1ce774c100882p-7:0x1.1ce774c100882p-7:0x1.7bdf4656ab602p-9" coeffa="-0x1.2fe25628eb285p+1:0x1.edea40cd1955ep+0:-0x1.106c2ec3d0af8p-1" lsbIn=-12 lsbOut=-12 TestBench n=10000


The easy IIR8
./flopoco FixIIR lsbin=-8 lsbout=-8 coeffa="-7.950492486500551e-01:1.316171195258671e+00:-6.348825276850145e-01:4.221584399465901e-01:-1.148028526411250e-01:3.337958526084077e-02:-3.947725765551162e-03:3.076933229437558e-04" coeffb="4.778650621278516e-03:3.822920497022813e-02:1.338022173957985e-01:2.676044347915969e-01:3.345055434894962e-01:2.676044347915969e-01:1.338022173957985e-01:3.822920497022813e-02:4.778650621278516e-03"

The difficult IIR8
./flopoco FixIIR lsbin=-8 lsbout=-8 coeffb="1.141178044605012e-02:-2.052394720689188e-02:4.328501070380458e-02:-4.176737791619380e-02:5.542991404945895e-02:-4.176737791619380e-02:4.328501070380458e-02:-2.052394720689188e-02:1.141178044605012e-02" coeffa="-5.081533236401664e+00:1.295349903984737e+01:-2.075814350661222e+01:2.265058884827988e+01:-1.712035157843729e+01:8.753862370563619e+00:-2.776525797340274e+00:4.237548320448786e-01"

*/

using namespace std;

namespace flopoco {

	FixIIR::FixIIR(OperatorPtr parentOp, Target* target, int lsbIn_, int lsbOut_,  vector<string> coeffb_, vector<string> coeffa_, double H_, double Heps_, bool buildWorstCaseTestBench_) :
		Operator(parentOp, target), lsbIn(lsbIn_), lsbOut(lsbOut_), coeffb(coeffb_), coeffa(coeffa_), H(H_), Heps(Heps_), buildWorstCaseTestBench(buildWorstCaseTestBench_)
	{
		srcFileName="FixIIR";
		setCopyrightString ( "Florent de Dinechin, Louis Beseme, Matei Istoan (2014-2019)" );
		useNumericStd_Unsigned();


		ostringstream name;
		name << "FixIIR";
		setNameWithFreqAndUID( name.str() );
		
		m = coeffa.size();
		n = coeffb.size();  

		addInput("X", 1-lsbIn, true);

		//Parsing the coefficients, into MPFR (and double for H but it is temporary)

		coeffa_d  = (double*) malloc(m * sizeof(double));
		coeffb_d  = (double*) malloc(n * sizeof(double));
		coeffa_mp = (mpfr_t*) malloc(m * sizeof(mpfr_t));
		coeffb_mp = (mpfr_t*) malloc(n * sizeof(mpfr_t));
		xHistory  = (mpfr_t*) malloc(n * sizeof(mpfr_t));
		yHistory  = (mpfr_t*) malloc((m+2) * sizeof(mpfr_t)); // We need to memorize the m previous y, and the current output. Plus one because it helps debugging


		for (uint32_t i=0; i< n; i++)		{
			// parse the coeffs from the string, with Sollya parsing
			sollya_obj_t node;

			node = sollya_lib_parse_string(coeffb[i].c_str());
			if(node == 0)					// If conversion did not succeed (i.e. parse error)
				THROWERROR("Unable to parse string " << coeffb[i] << " as a numeric constant");

			mpfr_init2(coeffb_mp[i], 10000);
			sollya_lib_get_constant(coeffb_mp[i], node);
			coeffb_d[i] = mpfr_get_d(coeffb_mp[i], GMP_RNDN);
			REPORT(DETAILED, "b[" << i << "]=" << setprecision(15) << scientific  << coeffb_d[i]);			
		}

		for (uint32_t i=0; i< m; i++)		{
			// parse the coeffs from the string, with Sollya parsing
			sollya_obj_t node;

			node = sollya_lib_parse_string(coeffa[i].c_str());
			if(node == 0)					// If conversion did not succeed (i.e. parse error)
				THROWERROR("Unable to parse string " << coeffb[i] << " as a numeric constant");
			
			mpfr_init2(coeffa_mp[i], 10000);
			sollya_lib_get_constant(coeffa_mp[i], node);
			coeffa_d[i] = mpfr_get_d(coeffa_mp[i], GMP_RNDN);
			REPORT(DETAILED, "a[" << i << "]=" << scientific << setprecision(15) << coeffa_d[i]);			
		}

		
		// TODO here compute H if it is not provided
		if(H==0 && Heps==0) {
#if HAVE_WCPG

			REPORT(INFO, "H not provided: computing worst-case peak gain");

			if (!WCPG_tf(&H, coeffb_d, coeffa_d, n, m, (int)0))
				THROWERROR("Could not compute WCPG");
			REPORT(INFO, "Computed filter worst-case peak gain: H=" << H);

			double one_d[1] = {1.0}; 
			if (!WCPG_tf(&Heps, one_d, coeffa_d, 1, m, (int)0))
				THROWERROR("Could not compute WCPG");
			REPORT(INFO, "Computed error amplification worst-case peak gain: Heps=" << Heps);
			
#else 
			THROWERROR("WCPG was not found (see cmake output), cannot compute worst-case peak gain H. Either provide H, or compile FloPoCo with WCPG");
#endif
		}
		else {
			REPORT(INFO, "Filter worst-case peak gain: H=" << H);
			REPORT(INFO, "Error amplification worst-case peak gain: Heps=" << Heps);
		}
		
		// guard bits for a faithful result
		int lsbExt = lsbOut-1-intlog2(Heps);


#if 0 // sabotaging, to test if we overestimate
		// The example with poles close to 1 still passes with lsbExt+=3, fails with lsbExt+=4
		// The easy IIR8 still passes with lsbExt+=2, fails with lsbExt+=3 
		// The trickiest IIR8 (with WCPG~8000) still passes with lsbExt+=3, fails with lsbExt+=4
		
	  lsbExt +=3 ;
#endif
		
		msbOut = ceil(log2(H)); // see the paper
		REPORT(INFO, "We ask for a SOPC faithful to lsbExt=" << lsbExt);
		REPORT(INFO, "msbOut=" << msbOut);


		// Initialisations for the emulate
		hugePrec = 10*(1+msbOut+-lsbOut+g);
		currentIndex=0x0FFFFFFFFFFFFFFFUL; // it will be decremented, let's start from high

		for (uint32_t i = 0; i<m+2; i++)
		{
			mpfr_init2 (yHistory[i], hugePrec);
			mpfr_set_d(yHistory[i], 0.0, GMP_RNDN);
		}
		for (uint32_t i=0; i<n; i++) {
			mpfr_init2 (xHistory[i], hugePrec);
			mpfr_set_d(xHistory[i], 0.0, GMP_RNDN);
		}

		// The instance of the shift register for Xd1...Xdn-1
		vhdl << tab << declare("U0", 1-lsbIn)  << " <= X;" << endl;
		string outportmap="";
		for(uint32_t i = 1; i<n; i++) {
			outportmap += join("Xd", i) + "=>" +  join("U", i) + (i<n-1?",":"") ;
		}
		newInstance("ShiftReg", "inputShiftReg",
								join("w=",1-lsbIn) + join(" n=", n-1) + " reset=1", // the parameters
								"X=>X", outportmap);  // the in and out port maps


		vhdl << tab << declare("Y0", msbOut-lsbExt+1) << " <= Yinternal;" << endl; // just so that the inportmap finds it  
		// The instance of the shift register for Yd1...Ydn-1
		outportmap = "";
		for (uint32_t i = 1; i<m+1; i++) {
			outportmap += join("Xd", i) + "=>" + join("Y", i) + (i<m?",":"");
		}
		newInstance("ShiftReg", "outputShiftReg",
								"w=" + to_string(msbOut-lsbExt+1) + " n=" + to_string(m) + " reset=1",
								"X=>Y0", outportmap);
		
		// Now building a single SOPC. For this we need the following info:
		//		FixSOPC(Target* target, vector<double> maxX, vector<int> lsbIn, int msbOut, int lsbOut, vector<string> coeff_, int g=-1);
		// We will concatenate coeffb of size n then then coeffa of size m
		// not using the newInstance() interface here.
		
		vector<double> maxInSOPC;
		vector<int> lsbInSOPC;
		vector<string> coeffSOPC;
		for (uint32_t i=0; i<n; i++) {
			maxInSOPC.push_back(1.0); // max(u) = 1.
			lsbInSOPC.push_back(lsbIn); // for u
			coeffSOPC.push_back(coeffb[i]);
		}
		for (uint32_t i = 0; i<m; i++)	{
			maxInSOPC.push_back(H); // max (y) = H. 
			lsbInSOPC.push_back(lsbExt); // for y
			coeffSOPC.push_back("-("+coeffa[i]+")");
		}
		schedule();
		for (uint32_t i=0; i<n; i++) {
			inPortMap(join("X",i), join("U", i));
		}
		for (uint32_t i = 0; i<m; i++)	{
			inPortMap(join("X",i+n), join("Y", i+1));
		}
		outPortMap( "R", "Yinternal");

		disablePipelining();

		FixSOPC* fixSOPC = new FixSOPC(this, getTarget(), maxInSOPC, lsbInSOPC, msbOut, lsbExt, coeffSOPC, -1); // -1 means: faithful
		vhdl << instance(fixSOPC, "fixSOPC", false /*this suppresses the "obsolete" warning*/ );

		enablePipelining();

		//The final rounding must be computed with an addition, no escaping it
		int sizeYinternal = msbOut - lsbExt + 1;
		int sizeYfinal = msbOut - lsbOut + 1;
		vhdl << tab << declare("Yrounded", sizeYfinal+1) <<  " <= (Yinternal" << range(sizeYinternal-1,  sizeYinternal-sizeYfinal-1) << ")  +  (" << zg(sizeYfinal)  << " & \"1\" );" << endl;

		addOutput("R", sizeYfinal,   true);
		vhdl << "R <= Yrounded" << range(msbOut-lsbOut+1, 1) << ";" << endl;
	};



	FixIIR::~FixIIR(){
		delete(coeffb_d);
		delete(coeffa_d);
		for (uint32_t i=0; i<n; i++) {
			mpfr_clear(coeffb_mp[i]);
			mpfr_clear(xHistory[i]);
		}
		for (uint32_t i=0; i<m; i++) {
			mpfr_clear(coeffa_mp[i]);
			mpfr_clear(xHistory[i]);
		}
		delete(coeffa_mp);
		delete(coeffb_mp);
		delete(xHistory);
		delete(yHistory);
	};




	void FixIIR::emulate(TestCase * tc){
		mpz_class sx;
		mpfr_t x, s, t;
		
		mpfr_init2 (s, hugePrec);
		mpfr_init2 (t, hugePrec);
		mpfr_set_d(s, 0.0, GMP_RNDN); // initialize s to 0

		mpfr_init2 (x, 1-lsbOut);

		sx = tc->getInputValue("X"); 		// get the input bit vector as an integer
		sx = bitVectorToSigned(sx, 1-lsbIn); 						// convert it to a signed mpz_class
		mpfr_set_z (x, sx.get_mpz_t(), GMP_RNDD); 				// convert this integer to an MPFR; this rounding is exact
		mpfr_div_2si (x, x, -lsbIn, GMP_RNDD); 						// multiply this integer by 2^-p to obtain a fixed-point value; this rounding is again exact
		mpfr_set(xHistory[currentIndex % n], x, GMP_RNDN); // exact


		// TODO CHECK HERE
		for (uint32_t i=0; i< n; i++)		{
			mpfr_mul(t, xHistory[(currentIndex+i)%n], coeffb_mp[i], GMP_RNDZ); 					// Here rounding possible, but precision used is ridiculously high so it won't matter
			mpfr_add(s, s, t, GMP_RNDN); 							// same comment as above
		}

		for (uint32_t i=0; i<m; i++)		{
			mpfr_mul(t, yHistory[(currentIndex +i+1)%(m+2)], coeffa_mp[i], GMP_RNDZ); 					// Here rounding possible, but precision used is ridiculously high so it won't matter
			mpfr_sub(s, s, t, GMP_RNDZ); 							// same comment as above
		}
		mpfr_set(yHistory[(currentIndex  +0)%(m+2)], s, GMP_RNDN);

#if 0// debugging the emulate
		cout << "x=" << 	mpfr_get_d(xHistory[currentIndex % n], GMP_RNDN);
		cout << " //// y=" << 	mpfr_get_d(s,GMP_RNDN) << "  ////// ";
		for (uint32_t i=0; i< n; i++)		{
			cout << "  x" << i<< "c" << i<<  "=" <<
				mpfr_get_d(xHistory[(currentIndex+i)%n],GMP_RNDN) << "*" << mpfr_get_d(coeffb_mp[i],GMP_RNDN);
		}
		cout << "  // ";
		for (uint32_t i=0; i<m; i++) {
			cout <<"  ya" << i+1 << "=" <<
				mpfr_get_d(yHistory[(currentIndex +i+1)%(m+2)],GMP_RNDN) << "*" << mpfr_get_d(coeffa_mp[i],GMP_RNDN);
		}
		cout << endl;
		  
#endif

		currentIndex--;

		//		coeff		  1 2 3
		//    yh      y 0 0 0 
		// now we should have in s the (exact in most cases) sum
		// round it up and down

		// debug: with this we observe if the simulation diverges
		double d =  mpfr_get_d(s, GMP_RNDD);
		miny=min(d,miny);
		maxy=max(d,maxy);
		//		cout << "y=" << d <<  "\t  log2(|y|)=" << (ceil(log2(abs(d)))) << endl;


		// make s an integer -- no rounding here
		mpfr_mul_2si (s, s, -lsbOut, GMP_RNDN);

		// We are waiting until the first meaningful value comes out of the IIR

		mpz_class rdz, ruz;
		mpfr_get_z (rdz.get_mpz_t(), s, GMP_RNDD); 					// there can be a real rounding here
#if 1 // to unplug the conversion that fails to see if it diverges further
		rdz=signedToBitVector(rdz, msbOut-lsbOut+1);
		tc->addExpectedOutput ("R", rdz);

		mpfr_get_z (ruz.get_mpz_t(), s, GMP_RNDU); 					// there can be a real rounding here
		ruz=signedToBitVector(ruz, msbOut-lsbOut+1);
		tc->addExpectedOutput ("R", ruz);
#endif
		
		mpfr_clears (x, t, s, NULL);

	};


	void FixIIR::computeImpulseResponse() {
		// simulate the filter on an impulsion for long enough, until
		// double threshold = 0.5/(1<<-lsbOut);
		double threshold = 0; //soyons fous
		double epsilon=1e15; // initialize with a large value
		uint64_t k;
		//initialize the ui and yi
		for(uint32_t i=0; i<n+m; i++) {
			ui.push_back(0);
			yi.push_back(0);		
		}
		ui.push_back(1); // input impulse
		yi.push_back(0);		

		k=0;
		int storageOffset=n+m;
		
		while (epsilon>threshold) {
			// make room
			ui.push_back(0);			
			yi.push_back(0);		
			// compute the new y
			double y=0;
			for(uint32_t i=0; i<n; i++) {
				y += ui[storageOffset+ k-i]*coeffb_d[i] ;
			}
			for(uint32_t i=0; i<m; i++) {
				//		cout << "    k=" << k <<" i=" << i <<  "  yi[storageOffset+ k-i] =" << yi[storageOffset+ k-i] << endl;  
				y -= yi[storageOffset+ k-i]*coeffa_d[i] ;
			}
			k++;
			yi[storageOffset+k] = y;
				 
			epsilon = abs(y);
			//cout << "k=" << k << " yi=" << y << endl;
			if(k>=300000){
				REPORT(0, "computeImpulseResponse: giving up for k=" <<k << " with epsilon still at " << epsilon << ", it seems hopeless");
				epsilon=0;
			}
		}
		vanishingK=k;
		REPORT(0, "Impulse response vanishes for k=" << k);
	}

	
	void FixIIR::buildStandardTestCases(TestCaseList* tcl){
		// First fill with a few ones, then a few zeroes
		TestCase *tc;

#if 1 // Test on the impulse response, useful for debugging 
		tc = new TestCase(this);
		tc->addInput("X", (mpz_class(1)<<(-lsbIn))-1 ); // 1 (almost)
		emulate(tc);
		tcl->add(tc);

		for (uint32_t i=0; i<100; i++) {
			tc = new TestCase(this);
			tc->addInput("X", mpz_class(0));
			emulate(tc);
			tcl->add(tc);
		}
		
#endif
		if(buildWorstCaseTestBench) {
			// compute the impulse response
			computeImpulseResponse();
			// Now fill with a signal that follows the sign alternance of the impulse response: this builds a worst-case signal
			miny=0; maxy=0;
			int storageOffset=n+m;
			uint32_t kmax = vanishingK-storageOffset;
			for (uint32_t i =0; i<kmax; i++) {
				mpz_class val;
#if 0
				if(yi[kmax-i]<0) {
					val = ((mpz_class(1)<<(-lsbIn)) -1) ; // 011111
				}
				else {
					val = ((mpz_class(1)<<(-lsbIn)) +1); // 100001
				}
#else // multiplying by 1 and -1 ensures no rounding error in the FIR part
			// hence the following code
			// But no observable difference... 
				val = ((mpz_class(1)<<(-lsbIn)) -1) * 9 / 10; // 011111;  *9/10 to trigger rounding errors
				if(yi[kmax-i]>=0) {
					// two's complement
					val = ((mpz_class(1)<<(-lsbIn+1)) -1) -val +1 ; // 111111 - val + 1
				}
#endif
				tc = new TestCase(this);
				tc->addInput("X", val); 
				emulate(tc);
				tcl->add(tc);
			
			}

			REPORT(0,"Filter output remains in [" << miny << ", " << maxy<<"]");
		}		
	};


	
	OperatorPtr FixIIR::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int lsbIn;
		UserInterface::parseInt(args, "lsbIn", &lsbIn);
		int lsbOut;
		UserInterface::parseInt(args, "lsbOut", &lsbOut);
		double h;
		UserInterface::parseFloat(args, "H", &h);
		double heps;
		UserInterface::parseFloat(args, "Heps", &heps);
		vector<string> inputa;
		string in;
		UserInterface::parseString(args, "coeffa", &in);
		bool buildWorstCaseTestBench;
		UserInterface::parseBoolean(args, "buildWorstCaseTestBench", &buildWorstCaseTestBench);
		
		// tokenize a string, thanks Stack Overflow
		stringstream ss(in);
		while( ss.good() )	{
				string substr;
				getline( ss, substr, ':' );
				inputa.push_back( substr );
			}

		vector<string> inputb;
		UserInterface::parseString(args, "coeffb", &in);
		stringstream ssb(in);
		while( ssb.good() )	{
				string substr;
				getline( ssb, substr, ':' );
				inputb.push_back( substr );
			}
		
		return new FixIIR(parentOp, target, lsbIn, lsbOut, inputb, inputa, h, heps, buildWorstCaseTestBench);
	}

	TestList FixIIR::unitTest(int index)
	{
		TestList testStateList;
		vector<pair<string,string>> paramList;
		
	if(index==-1)
		{ // The unit tests
			// No parameter sweep here, just a few representative examples

			// A Butterworth
			paramList.push_back(make_pair("lsbIn",  "-12"));
			paramList.push_back(make_pair("lsbOut", "-12"));
			paramList.push_back(make_pair("coeffb",  "\"0x1.7bdf4656ab602p-9:0x1.1ce774c100882p-7:0x1.1ce774c100882p-7:0x1.7bdf4656ab602p-9\""));
			paramList.push_back(make_pair("coeffa",  "\"-0x1.2fe25628eb285p+1:0x1.edea40cd1955ep+0:-0x1.106c2ec3d0af8p-1\""));
			testStateList.push_back(paramList);
			paramList.clear();

			// The example with poles close to 0
			paramList.push_back(make_pair("lsbIn",  "-12"));
			paramList.push_back(make_pair("lsbOut", "-12"));
			paramList.push_back(make_pair("coeffb",  "\"0x1.89ff611d6f472p-13:-0x1.2778afe6e1ac0p-11:0x1.89f1af73859fap-12:0x1.89f1af73859fap-12:-0x1.2778afe6e1ac0p-11:0x1.89ff611d6f472p-13\""));
			paramList.push_back(make_pair("coeffa",  "\"-0x1.3f4f52485fe49p+2:0x1.3e9f8e35c8ca8p+3:-0x1.3df0b27610157p+3:0x1.3d42bdb9d2329p+2:-0x1.fa89178710a2bp-1\""));
			testStateList.push_back(paramList);
			paramList.clear();

		}
	else
		{
			// finite number of random test computed out of index
		}

	return testStateList;
	}


	
	void FixIIR::registerFactory(){
		UserInterface::add("FixIIR", // name
											 "A fix-point Infinite Impulse Response filter generator.",
											 "FiltersEtc", // categories
											 "",
											 "lsbIn(int): input most significant bit;\
                        lsbOut(int): output least significant bit;\
                        H(real)=0: worst-case peak gain. if 0, it will be computed by the WCPG library;\
                        Heps(real)=0: worst-case peak gain of the feedback loop. if 0, it will be computed by the WCPG library;\
                        coeffa(string): colon-separated list of real coefficients using Sollya syntax. Example: coeffa=\"1.234567890123:sin(3*pi/8)\";\
                        coeffb(string): colon-separated list of real coefficients using Sollya syntax. Example: coeffb=\"1.234567890123:sin(3*pi/8)\";\
                        buildWorstCaseTestBench(bool)=false: if true, the TestBench for this IIR will begin with a stimulation by the worst-case input signal",
											 "",
											 FixIIR::parseArguments,
											 FixIIR::unitTest
											 ) ;
	}

}
