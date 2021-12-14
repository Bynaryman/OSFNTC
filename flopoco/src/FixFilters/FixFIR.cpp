#include <iostream>
#include <iomanip>
#include <sstream>

#include "gmp.h"
#include "mpfr.h"
#include "sollya.h"

#include "FixFIR.hpp"

#include "ShiftReg.hpp"

using namespace std;

namespace flopoco {

	const int veryLargePrec = 6400;  /*6400 bits should be enough for anybody */

	FixFIR::FixFIR(OperatorPtr parentOp, Target* target, int lsbIn_, int lsbOut_):
		Operator(parentOp, target), lsbIn(lsbIn_), lsbOut(lsbOut_)
	{
		initFilter();
	};


	FixFIR::FixFIR(OperatorPtr parentOp, Target* target, int lsbIn_, int lsbOut_, vector<string> coeff_, int symmetry_, bool rescale_) :
		Operator(parentOp, target), lsbIn(lsbIn_), lsbOut(lsbOut_), coeff(coeff_), symmetry(symmetry_), rescale(rescale_)
	{
			initFilter();
			buildVHDL();
	};


	void FixFIR::initFilter(){
		srcFileName="FixFIR";
		setCopyrightString ( "Louis Besème, Matei Istoan, Florent de Dinechin (2013-2017)" );

		ostringstream name;
		name << "FixFIR_uid" << getNewUId();
		setNameWithFreqAndUID( name.str() );

		sollya_lib_set_roundingwarnings(sollya_lib_parse_string("off"));
	}


	// The method that does the work once coeff[] is known
	//	with support for symmetric coefficients (if properly declared)
	void FixFIR::buildVHDL(){
		n=coeff.size();

		useNumericStd_Unsigned();
		if(-lsbIn<1) {
			THROWERROR("Can't build an architecture for this value of lsbIn: " << lsbIn)
		}
		addInput("X", 1-lsbIn, true);


		// The shift register
		vhdl << tab << declare("Xd0", 1-lsbIn)  << " <= X;" << endl;
		// The instance of the shift register for Xd1...Xdn-1
		string omap="";
		for(int i = 1; i<n; i++) {
			omap += join("Xd", i) + "=>" +  join("Xd", i) + (i<n-1?",":"") ;
		}
		newInstance("ShiftReg", "inputShiftReg",
								join("w=",1-lsbIn) + join(" n=", n-1) + join(" reset=", 1), // the parameters
								"X=>X", omap);  // the in and out port maps

		
		double sumAbs;
		// Most of this code is copypasted from SOPC, which is redundant: to fix some day if it doesn't jeopardize readability.
		// parse the coeffs from the string, with Sollya parsing
		mpfr_t sumAbsCoeff, mpfrCoeff;
		mpfr_init2 (sumAbsCoeff, 1-lsbIn);
		mpfr_set_d (sumAbsCoeff, 0.0, GMP_RNDN);
		
		for (int i=0; i< n; i++)	{
			mpfr_init2 (mpfrCoeff, veryLargePrec);
			sollya_obj_t node;
			node = sollya_lib_parse_string(coeff[i].c_str());
			// If conversion did not succeed (i.e. parse error)
			if(node == 0)	{
				ostringstream error;
				error << srcFileName << ": Unable to parse string " << coeff[i] << " as a numeric constant" << endl;
				throw error.str();
			}
			mpfr_init2(mpfrCoeff, veryLargePrec);
			sollya_lib_get_constant(mpfrCoeff, node);
			sollya_lib_clear_obj(node);

			// store the coefficient as a double, for error reporting etc
			coeffD.push_back(mpfr_get_d(mpfrCoeff, GMP_RNDN));
			// Accumulate the absolute values
			mpfr_abs(mpfrCoeff, mpfrCoeff, GMP_RNDU);
			mpfr_add(sumAbsCoeff, sumAbsCoeff, mpfrCoeff, GMP_RNDU);
			mpfr_clears(mpfrCoeff, NULL);
		}
		// now sumAbsCoeff is the max value that the filter can take.
		sumAbs = mpfr_get_d(sumAbsCoeff, GMP_RNDU); // round up so that we round towards zero the division
		mpfr_clears(sumAbsCoeff, NULL);

		if(rescale){
			REPORT(INFO, "Scaling all the coefficients by 1/" << sumAbs);			
			// now just replace each coeff with the scaled version
			for (int i=0; i< n; i++)	{
				ostringstream s;
				s << "(" << coeff[i] << ")/" << setprecision(20) << sumAbs;
				coeff[i] = s.str();
			}
			sumAbs=0.999999999999999;
		}

		// Symmetry checks
		if(symmetry!=0) {
			for (int i=0; i< n/2; i++)	{ // n/2 is floor(n/2) 
				if(symmetry==1) {
					if (coeffD[i]!=coeffD[n-i-1]) {
						REPORT(0, endl << endl  << " **** WARNING **** This filter is supposed to be symmetric, but coefficients " << i << " and " << n-i-1
									 << " evaluate respectively to " << endl << setprecision(16) << "     " << coeffD[i] << endl << " and " << coeffD[n-i-1]
									 << endl << " This is could be a numerical artifact, who am I to decide if it is significant to your computation ?"
									 << endl << " However this discrepancy may prevent TestBench to succeed, and I encourage you to fix it." << endl );									 
					}
				}
				if(symmetry==-1) {
					if (coeffD[i]!=-coeffD[n-i-1]) {
						REPORT(0, endl << endl  << " **** WARNING **** This filter is supposed to be anti-symmetric, but coefficients " << i << " and " << n-i-1
									 << " evaluate respectively to " << endl << setprecision(16) << "     " << coeffD[i] << endl << " and " << coeffD[n-i-1]
									 << endl << " This is could be a numerical artifact, who am I to decide if it is significant to your computation ?"
									 << endl << " However this discrepancy may prevent TestBench to succeed, and I encourage you to fix it."  << endl );									 
					}
				}
			}
			if((symmetry==-1) && (n%2 == 1)) {
				if (coeffD[n/2]!=0) {
					REPORT(0, endl << endl << " **** WARNING **** This filter is supposed to be anti-symmetric, but coefficient " << n/2 << " evaluates to "
								 << setprecision(16) << "  " << coeffD[n/2] << "   which is not exactly equal to zero." 
								 << endl << " This is could be a numerical artifact, who am I to decide if it is significant to your computation ?"
								 << endl << " However this discrepancy may prevent TestBench to succeed, and I encourage you to fix it." << endl );									 
				}
			}
		} // End symmetry checks

		// now sumAbs is the max value that the SOPC can take.
		REPORT(DETAILED, "sumAbs=" << sumAbs);
		msbOut = 1;
		while(sumAbs>=2.0){
			sumAbs*=0.5;
			msbOut++;
		}
		while(sumAbs<1.0){
			sumAbs*=2.0;
			msbOut--;
		}
		REPORT(INFO, "Computed msbOut=" << msbOut);
	
		// prepare the strings for newInstance()
		string inportmap = "";
		string coeffString = "";
		string msbInString = "";
		string lsbInString = "";
		string parameters;
		
		if(symmetry!=0)		{
			/* To exploit the symmetry, we must
			 1/ pre-add the symmetric inputs
			 2/ create the half-size list of coefficients: coeffSymmetric
			 3/ create a smaller SOPC which is called fixSOPC
			The preaddition adds one MSB bit, so input numbers are in [-2, 2], except the middle one.
			We have to pass this information to SOPC
			*/
			vhdl << endl;
			// The instance of SOPC
			int i;
			for (i=0; i< n/2; i++)	{ // n/2 is floor(n/2) 
				vhdl << tab << declare(join("PreSum", i),  1-lsbIn+1, true) << " <= "
						 << "(Xd" << i << "(" << 0-lsbIn << ") & Xd" << i << ")"
						 << (symmetry==1 ? " + " : " - ")
						 << "(Xd" << n-i-1 << "(" << 0-lsbIn << ") & Xd" << n-i-1 << ");" << endl;
				coeffString += coeff[i] + (i<n/2-1? ":":"");
				msbInString += to_string(1) + (i<n/2-1? ":":""); // msb for these terms is 1
				lsbInString += to_string(lsbIn) + (i<n/2-1? ":":"");
				inportmap += join("X", i) + "=>" +  join("PreSum", i) +  (i<n/2-1?",":"") ;
			}
			if(n%2 == 1) {
				coeffString += ":" + coeff[i];
				msbInString += ":" + to_string(0); // msb for this terms is 0
				lsbInString += ":" + to_string(lsbIn);
				inportmap += "," + join("X", i) + "=>" +  join("Xd", i);
			}			
			parameters = "msbIn=" + msbInString
				+ " lsbIn=" + lsbInString 
				+ join(" msbOut=", msbOut)
				+ join(" lsbOut=", lsbOut)
				+ " coeff=" + coeffString;
		}



		else { // normal, not symmetric case
			// The instance of SOPC 
			for (int i=0; i< n; i++)	{
				coeffString += coeff[i] + (i<n-1? ":":"");
				msbInString += to_string(0) + (i<n-1? ":":"");
				lsbInString += to_string(lsbIn) + (i<n-1? ":":"");
				inportmap += join("X", i) + "=>" +  join("Xd", i) +  (i<n-1?",":"") ;
			}
			
			parameters = "msbIn=" + msbInString
				+ " lsbIn=" + lsbInString 
				+ join(" msbOut=", msbOut)
				+ join(" lsbOut=", lsbOut)
				+ " coeff=" + coeffString;

		}

		fixSOPC = (FixSOPC*) newInstance("FixSOPCfull", "SOPC",
																		 parameters, // the parameters
																		 inportmap, // the in port maps
																		 "R=>Rtmp" );  // the out port map

		if(symmetry!=0){
			// For the emulate() computation we need to build the standard SOPC that doesn't exploit symmetry
			// TODO get rid of its svg output...
			UserInterface::pushAndClearGlobalOpList();
			refFixSOPC = new FixSOPC(nullptr, getTarget(), lsbIn, msbOut, lsbOut, coeff);
			REPORT(INFO, "Created reference SOPC called " << refFixSOPC->getName() );
			UserInterface::popGlobalOpList();
		}
		else {
			refFixSOPC = fixSOPC;
		}

		addOutput("R", fixSOPC->msbOut - fixSOPC->lsbOut + 1,   true);
		vhdl << tab << "R <= Rtmp;" << endl;

		
		// initialize stuff for emulate
		for(int i=0; i<=n; i++) {
			xHistory[i]=0;
		}
		currentIndex=0;
	};




	FixFIR::~FixFIR(){
	};



	void FixFIR::emulate(TestCase * tc){
		mpz_class sx = tc->getInputValue("X"); 		// get the input bit vector as an integer
		xHistory[currentIndex] = sx;

		// Not completely optimal in terms of object copies...
		vector<mpz_class> inputs;
		for (int i=0; i< n; i++)	{
			sx = xHistory[(currentIndex+n-i)%n];
			inputs.push_back(sx);
		}
		pair<mpz_class,mpz_class> results = refFixSOPC-> computeSOPCForEmulate(inputs);

		tc->addExpectedOutput ("R", results.first);
		tc->addExpectedOutput ("R", results.second);
		currentIndex=(currentIndex+1)%n; //  circular buffer to store the inputs

	};

	OperatorPtr FixFIR::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int lsbIn;
		UserInterface::parseInt(args, "lsbIn", &lsbIn);
		int lsbOut;
		UserInterface::parseInt(args, "lsbOut", &lsbOut);
		int symmetry;
		UserInterface::parseInt(args, "symmetry", &symmetry);
		bool rescale;
		UserInterface::parseBoolean(args, "rescale", &rescale);
		vector<string> coeffs;
		UserInterface::parseColonSeparatedStringList(args, "coeff", &coeffs);

		OperatorPtr tmpOp = new FixFIR(parentOp, target, lsbIn, lsbOut, coeffs, symmetry, rescale);

		return tmpOp;
	}

	void FixFIR::registerFactory(){
		UserInterface::add("FixFIR", // name
											 "A fix-point Finite Impulse Filter generator.",
											 "FiltersEtc", // categories
											 "",
											 "lsbIn(int): integer size in bits;\
											 lsbOut(int): integer size in bits;								\
           						 symmetry(int)=0: 0 for normal filter, 1 for symmetric, -1 for antisymmetric. If not 0, only the first half of the coeff list is used.; \
                       rescale(bool)=false: If true, divides all coefficients by 1/sum(|coeff|);\
                       coeff(string): colon-separated list of real coefficients using Sollya syntax. Example: coeff=\"1.234567890123:sin(3*pi/8)\"",
											 "For more details, see <a href=\"bib/flopoco.html#DinIstoMas2014-SOPCJR\">this article</a>.",
											 FixFIR::parseArguments
											 ) ;
	}


}

