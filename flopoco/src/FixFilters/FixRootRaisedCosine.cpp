#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "FixRootRaisedCosine.hpp"

using namespace std;

namespace flopoco{


	FixRootRaisedCosine::FixRootRaisedCosine(OperatorPtr parentOp_, Target* target_, int lsbIn_, int lsbOut_, int N_, double alpha_) :
		FixFIR(parentOp_, target_, lsbIn_, lsbOut_), N(N_)
	{
		srcFileName="FixRootRaisedCosine";
		
		ostringstream name;
		name << "FixRootRaisedCosine_lsbIn" << -lsbIn  << "_lsbOut" << -lsbOut  << "_N" << N << "_rolloff" << alpha;
		setNameWithFreqAndUID(name.str());

		setCopyrightString("Louis Besème, Florent de Dinechin, Matei Istoan (2014-2020)");

		// FROM THERE ON I AM NOT SURE
		// MPFR code for computing coeffs is  in Attic/FixRCF
		
		symmetry = -1; // this is a FIR attribute, Root-Raised Cosine is a symmetric filter
		rescale=true; // also a FIR attribute
		// define the coefficients

		THROWERROR("WORK IN PROGRESS, sorry, ask nicely");
		buildVHDL();
	};

	FixRootRaisedCosine::~FixRootRaisedCosine(){}

	OperatorPtr FixRootRaisedCosine::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int lsbIn;
		UserInterface::parseInt(args, "lsbIn", &lsbIn);
		int lsbOut;
		UserInterface::parseInt(args, "lsbOut", &lsbOut);
		int n;
		UserInterface::parseStrictlyPositiveInt(args, "n", &n);
		double alpha;
		UserInterface::parseFloat(args, "alpha", &alpha);	
		OperatorPtr tmpOp = new FixRootRaisedCosine(parentOp, target, lsbIn, lsbOut, n, alpha);
		return tmpOp;
	}


	TestList FixRootRaisedCosine::unitTest(int index)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;
		
		if(index==-1) 
		{ // The unit tests
			for(int lsbIn=-1; lsbIn>=-12; lsbIn-=1) {
				for(int lsbOut = min(lsbIn,-5); lsbOut>=-18; lsbOut-=3)	{
					for(int n = 3; n<8; n++)	{
						paramList.push_back(make_pair("lsbIn",to_string(lsbIn)));
						paramList.push_back(make_pair("lsbOut",to_string(lsbOut)));
						paramList.push_back(make_pair("n",to_string(n)));
						testStateList.push_back(paramList);
						paramList.clear();
					}					
				}
			}
		}
		else     
		{
				// finite number of random test computed out of index
			// TODO
		}	
		return testStateList;
	}


	
	void FixRootRaisedCosine::registerFactory(){
		UserInterface::add("FixRootRaisedCosine", // name
											 "A generator of fixed-point Root-Raised Cosine filters",
											 "FiltersEtc", // categories
											 "",
											 "alpha(real): roll-off factor;\
                        lsbIn(int): weight of the integer size in bits;							\
											  lsbOut(int): integer size in bits;\
                        n(int): filter order (number of taps will be 2n+1)",
											 "",
											 FixRootRaisedCosine::parseArguments,
											 FixRootRaisedCosine::unitTest
											 ) ;
	}

}


