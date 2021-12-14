/*
 * Faithful multiplication of a fixed point number by a real constant.
 *        Internally it is realized as a wrapper for FixRealKCM and FixRealShiftAdd.
 *
 * This file is part of the FloPoCo project developed by the Arenaire
 * team at Ecole Normale Superieure de Lyon
 *
 * Authors : Martin Kumm, martin.kumm@cs.hs-fulda.de
 *
 * Initial software.
 * Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
 * 2008-2011.
 * All rights reserved.
 */

#include "FixRealConstMult.hpp"

using namespace std;


namespace flopoco
{


FixRealConstMult::FixRealConstMult(OperatorPtr parentOp, Target *target, bool signedIn_, int msbIn_, int lsbIn_, int lsbOut_, string constant_, double targetUlpError_, FixRealConstMult::Method method_):
		FixRealConstMult(parentOp, target, signedIn_, msbIn_, lsbIn_, lsbOut_, constant_, targetUlpError_)
{
	constStringToSollya(); //necessary to update variables for emulate and setting of msbC

	msbOut = msbIn_ + msbC;
	int wIn = msbIn_ - lsbIn_ + 1;
	int wOut = msbOut - lsbOut_ + 1;

	if(method_ == automatic)
	{
		//automatic selection is chosen, so select the algorithm
		method_ = KCM; //dummy for a more clever selection
	}


	srcFileName = "FixRealConstMult";
	setNameWithFreqAndUID("FixRealConstMult");

	addInput("X", wIn);
	addOutput("R", wOut);

	// Now we can check when this is a multiplier by 0: either because the it is zero, or because it is close enough
	if (mpfr_zero_p(mpC) != 0)
	{
		REPORT(INFO, "It seems somebody asked for a multiplication by 0. We can do that.");
		vhdl << tab << "R <= (others => '0');" << endl;
		return;
	}

	const string parameters = "signedIn=" + to_string(signedIn_) + " msbIn=" + to_string(msbIn_) + " lsbIn=" + to_string(lsbIn_) + " lsbOut=" + to_string(lsbOut_) + " constant=" + constant_ + " targetUlpError=" + to_string(targetUlpError_);
	const string inPortMaps = "X=>X";
	const string outPortMaps = "R=>R";

	switch(method_)
	{
		case KCM:
			newInstance("FixRealKCM", "FixRealKCMInst", parameters, inPortMaps, outPortMaps);

			break;
		case ShiftAdd:
#if defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB) && defined(HAVE_SCALP)
			newInstance("FixRealShiftAdd", "FixRealShiftAddInst", parameters, inPortMaps, outPortMaps);
#else
			THROWERROR("Error: Method 'ShiftAdd' only available when FloPoCo is compiled against paglib, rpaglib and ScaLP libraries.");
#endif
			break;
		case automatic:
			//does not happen
			THROWERROR("Error: Automatic selection went wrong for some reasons.");
	}


}

FixRealConstMult::FixRealConstMult(OperatorPtr parentOp, Target *target, bool signedIn_, int msbIn_, int lsbIn_, int lsbOut_, string constant_, double targetUlpError_) :
		Operator(parentOp, target),
		signedIn(signedIn_),
		msbIn(msbIn_),
		lsbIn(lsbIn_),
		lsbOut(lsbOut_),
		constant(constant_),
		targetUlpError(targetUlpError_)
{

}

FixRealConstMult::FixRealConstMult(Operator* parentOp, Target* target) : Operator(parentOp, target)
{

}

void FixRealConstMult::constStringToSollya()
{
	// Convert the input string into a sollya evaluation tree
	sollya_obj_t sollyiaObj;
	sollyiaObj = sollya_lib_parse_string(constant.c_str());
	/* If  parse error throw an exception */
	if (sollya_lib_obj_is_error(sollyiaObj))
	{
		ostringstream error;
		error << srcFileName << ": Unable to parse string " <<
			  constant << " as a numeric constant" << endl;
		throw error.str();
	}

	mpfr_init2(mpC, 10000);
	mpfr_init2(absC, 10000);

	sollya_lib_get_constant(mpC, sollyiaObj);

	//if negative constant, then set negativeConstant
	negativeConstant = (mpfr_cmp_si(mpC, 0) < 0 ? true : false);

	signedOutput = negativeConstant || signedIn;
	mpfr_abs(absC, mpC, GMP_RNDN);

	REPORT(DEBUG, "Constant evaluates to " << mpfr_get_d(mpC, GMP_RNDN));

	//compute the logarithm only of the constants
	mpfr_t log2C;
	mpfr_init2(log2C, 100); // should be enough for anybody
	mpfr_log2(log2C, absC, GMP_RNDN);
	msbC = mpfr_get_si(log2C, GMP_RNDU);
	mpfr_clears(log2C, NULL);
}

TestList FixRealConstMult::unitTest(int index)
{
	// the static list of mandatory tests
	TestList testStateList;
	vector<pair<string,string>> paramList;

	if(index==-1)
	{ // The unit tests

		vector<string> constantList; // The list of constants we want to test
		constantList.push_back("\"0\"");
		constantList.push_back("\"0.125\"");
		constantList.push_back("\"-0.125\"");
		constantList.push_back("\"4\"");
		constantList.push_back("\"-4\"");
		constantList.push_back("\"log(2)\"");
		constantList.push_back("-\"log(2)\"");
		constantList.push_back("\"0.00001\"");
		constantList.push_back("\"-0.00001\"");
		constantList.push_back("\"0.0000001\"");
		constantList.push_back("\"-0.0000001\"");
		constantList.push_back("\"123456\"");
		constantList.push_back("\"-123456\"");

		for(int signedIn=1; signedIn>=0; signedIn--) {
			string signedInStr = to_string(signedIn);
			for(int wIn=3; wIn<16; wIn+=4) { // test various input widths
				for(int lsbIn=-1; lsbIn<2; lsbIn++) { // test various lsbIns
					string lsbInStr = to_string(lsbIn);
					string msbInStr = to_string(lsbIn+wIn);
					for(int lsbOut=-1; lsbOut<2; lsbOut++) { // test various lsbIns
						string lsbOutStr = to_string(lsbOut);
						for(auto c:constantList) { // test various constants
							paramList.push_back(make_pair("lsbIn",  lsbInStr));
							paramList.push_back(make_pair("lsbOut", lsbOutStr));
							paramList.push_back(make_pair("msbIn",  msbInStr));
							paramList.push_back(make_pair("signedIn", signedInStr));
							paramList.push_back(make_pair("constant", c));
							testStateList.push_back(paramList);
							paramList.clear();
						}
					}
				}
			}
		}
	}
	else
	{
		// finite number of random test computed out of index
	}

	return testStateList;
}

// TODO manage correctly rounded cases, at least the powers of two
// To have MPFR work in fix point, we perform the multiplication in very
// large precision using RN, and the RU and RD are done only when converting
// to an int at the end.
void FixRealConstMult::emulate(TestCase* tc)
{
	// Get I/O values
	mpz_class svX = tc->getInputValue("X");
	bool negativeInput = false;
	int wIn=msbIn-lsbIn+1;
	int wOut=msbOut-lsbOut+1;

	// get rid of two's complement
	if(signedIn)	{
		if ( svX > ( (mpz_class(1)<<(wIn-1))-1) )	 {
			svX -= (mpz_class(1)<<wIn);
			negativeInput = true;
		}
	}

	// Cast it to mpfr
	mpfr_t mpX;
	mpfr_init2(mpX, msbIn-lsbIn+2);
	mpfr_set_z(mpX, svX.get_mpz_t(), GMP_RNDN); // should be exact

	// scale appropriately: multiply by 2^lsbIn
	mpfr_mul_2si(mpX, mpX, lsbIn, GMP_RNDN); //Exact

	// prepare the result
	mpfr_t mpR;
	mpfr_init2(mpR, 10*wOut);

	// do the multiplication
	mpfr_mul(mpR, mpX, mpC, GMP_RNDN);

	// scale back to an integer
	mpfr_mul_2si(mpR, mpR, -lsbOut, GMP_RNDN); //Exact
	mpz_class svRu, svRd;

	mpfr_get_z(svRd.get_mpz_t(), mpR, GMP_RNDD);
	mpfr_get_z(svRu.get_mpz_t(), mpR, GMP_RNDU);

	//		cout << " emulate x="<< svX <<"  before=" << svRd;
	if(negativeInput != negativeConstant)		{
		svRd += (mpz_class(1) << wOut);
		svRu += (mpz_class(1) << wOut);
	}
	//		cout << " emulate after=" << svRd << endl;

	//Border cases
	if(svRd > (mpz_class(1) << wOut) - 1 )		{
		svRd = 0;
	}

	if(svRu > (mpz_class(1) << wOut) - 1 )		{
		svRu = 0;
	}

	tc->addExpectedOutput("R", svRd);
	tc->addExpectedOutput("R", svRu);

	// clean up
	mpfr_clears(mpX, mpR, NULL);
}

OperatorPtr FixRealConstMult::parseArguments(OperatorPtr parentOp, Target* target, std::vector<std::string> &args)
{
	int lsbIn, lsbOut, msbIn;
	bool signedIn;
	double targetUlpError;
	string constant;
	string methodStr;
	FixRealConstMult::Method method;

	UserInterface::parseInt(args, "lsbIn", &lsbIn);
	UserInterface::parseString(args, "constant", &constant);
	UserInterface::parseInt(args, "lsbOut", &lsbOut);
	UserInterface::parseInt(args, "msbIn", &msbIn);
	UserInterface::parseBoolean(args, "signedIn", &signedIn);
	UserInterface::parseFloat(args, "targetUlpError", &targetUlpError);
	UserInterface::parseString(args, "method", &methodStr);

	std::transform(methodStr.begin(), methodStr.end(), methodStr.begin(), ::tolower);


	if(methodStr.compare("auto") == 0)
	{
		method = FixRealConstMult::automatic;
	}
	else if(methodStr.compare("kcm") == 0)
	{
		method = FixRealConstMult::KCM;
	}
	else if(methodStr.compare("shiftadd") == 0)
	{
		method = FixRealConstMult::ShiftAdd;
	}
	else
	{
		throw("Error: Unknown method " + methodStr);
	}

	return new FixRealConstMult(
			parentOp,
			target,
			signedIn,
			msbIn,
			lsbIn,
			lsbOut,
			constant,
			targetUlpError,
			method
	);
}


void flopoco::FixRealConstMult::registerFactory()	{
	UserInterface::add(
			"FixRealConstMult",
			"Table based real multiplier. Output size is computed",
			"ConstMultDiv",
			"",
			"signedIn(bool): 0=unsigned, 1=signed; \
			msbIn(int): weight associated to most significant bit (including sign bit);\
			lsbIn(int): weight associated to least significant bit;\
			lsbOut(int): weight associated to output least significant bit; \
			constant(string): constant given in arbitrary-precision decimal, or as a Sollya expression, e.g \"log(2)\"; \
			targetUlpError(real)=1.0: required precision on last bit. Should be strictly greater than 0.5 and lesser than 1; \
			method(string)=auto: desired method. Can be 'KCM', 'ShiftAdd' or 'auto' (let FloPoCo decide which operator performs best)",
			"The KCM variant is described in <a href=\"bib/flopoco.html#volkova:hal-01561052\">this article</a>. The Shift-and-Add variant is described  in <a href=\"bib/flopoco.html#deDinechinEtAl2019-Arith-KCMvsSA\">this article</a>.<br> Special constants, such as 0 or powers of two, are handled efficiently.",
			FixRealConstMult::parseArguments,
			FixRealConstMult::unitTest
	);
}

}
