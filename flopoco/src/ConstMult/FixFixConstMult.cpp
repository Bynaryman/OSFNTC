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

#include "FixFixConstMult.hpp"

using namespace std;


namespace flopoco
{


	FixFixConstMult::FixFixConstMult(OperatorPtr parentOp, Target *target, bool signedIn_, int msbIn_, int lsbIn_, int lsbOut_, string constant_, double targetUlpError_):
			Operator(parentOp, target)
	{

#if defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB) && defined(HAVE_SCALP)
#endif

	}

	TestList FixFixConstMult::unitTest(int index)
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

			for(int wIn=3; wIn<16; wIn+=4) { // test various input widths
				for(int lsbIn=-1; lsbIn<2; lsbIn++) { // test various lsbIns
					string lsbInStr = to_string(lsbIn);
					string msbInStr = to_string(lsbIn+wIn);
					for(int lsbOut=-1; lsbOut<2; lsbOut++) { // test various lsbIns
						string lsbOutStr = to_string(lsbOut);
						for(int signedIn=0; signedIn<2; signedIn++) {
							string signedInStr = to_string(signedIn);
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

	OperatorPtr FixFixConstMult::parseArguments(OperatorPtr parentOp, Target* target, std::vector<std::string> &args)
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
		return new FixFixConstMult(
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


	void flopoco::FixFixConstMult::registerFactory()	{
		UserInterface::add(
				"FixFixConstMult",
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
				FixFixConstMult::parseArguments,
				FixFixConstMult::unitTest
		);
	}

}