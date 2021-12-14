/*
  Integer constant multiplication using minimum number of adders due to

  Gustafsson, O., Dempster, A., Johansson, K., Macleod, M., & Wanhammar, L. (2006).
  Simplified Design of Constant Coefficient Multipliers. Circuits, Systems, and Signal Processing, 25(2), 225–251.

  All constants up to 19 bit will be realized optimal using precomputed tables provided by the SPIRAL project (http://spiral.ece.cmu.edu/mcm/).
  
  Author : Martin Kumm kumm@uni-kassel.de, (emulate adapted from Florent de Dinechin, Florent.de.Dinechin@ens-lyon.fr)

  All rights reserved.

*/
#if defined(HAVE_PAGLIB) && defined(HAVE_OSCM) && defined(HAVE_SCALP)

#include <iostream>
#include <sstream>
#include <vector>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "../utils.hpp"
#include "../Operator.hpp"

#include "IntConstMultShiftAddOpt.hpp"

#include "pagsuite/scm_solutions.hpp"
#include "pagsuite/pagexponents.hpp"
#include "pagsuite/compute_successor_set.h"
#include "pagsuite/log2_64.h"
#include "pagsuite/fundamental.h"

#include <algorithm>
#include <cassert>

using namespace std;
using namespace PAGSuite;

namespace flopoco{

    IntConstMultShiftAddOpt::IntConstMultShiftAddOpt(Operator* parentOp, Target* target, int wIn, int coeff, bool syncInOut, int epsilon)  : IntConstMultShiftAdd(parentOp, target, wIn, "", false, syncInOut, 1000, false, epsilon)
    {
		srcFileName="IntConstMultShiftAddOpt";

        this->coeff = coeff;

        cout << "wIn=" << wIn << ", coeff=" << coeff << endl;


        if(coeff <= 0)
        {
		  cerr << "Error: Value of constant must be positive and non-zero" << endl;
          exit(-1);
        }

        int shift;
        int coeffOdd = fundamental(coeff, &shift);
		int outputStage; //stage of the output layer

        if((coeffOdd <= MAX_SCM_CONST) && (coeffOdd >= 1))
        {
        	if(coeffOdd > 1)
			{
				buildAdderGraph(coeffOdd);
				outputStage=1;
			}
			else //coeffOdd == 1
			{
				outputStage=0;
			}
        }
        else
        {
		  cerr << "Error: (Odd) value of constant must be less than " << MAX_SCM_CONST << " (is " << coeffOdd << ")" << endl;
          exit(-1);
        }

		//add output:
		adderGraph << "{'O',[" << coeff << "]," << outputStage << ",[" << coeffOdd << "]," << outputStage << "," << shift << "},";

        stringstream adderGraphComplete;
        adderGraphComplete << "{" << adderGraph.str().substr(0,adderGraph.str().length()-1) << "}";

        REPORT(
				INFO,
				"adder_graph=" << adderGraphComplete.str()
                );

		ProcessIntConstMultShiftAdd(target,adderGraphComplete.str(),"",epsilon);

        ostringstream name;
        name << "IntConstMultShiftAddOpt_" << coeff << "_" << wIn;
        setName(name.str());
    }

    void IntConstMultShiftAddOpt::generateAOp(int a, int b, int c, int eA, int eB, int signA, int signB, int preFactor)
	{
		string signAStr,signBStr;

		if(signA < signB)
		{
			//swap a and b:
			int t;
			t=a; a=b; b=t;
			t=eA; eA=eB; eB=t;
			t=signA; signA=signB; signB=t;
		}

		if(signA > 0) signAStr = ""; else signAStr = "-";
		if(signB > 0) signBStr = ""; else signBStr = "-";

		int coeffStage=1; //its all combinatorial, so the adders are defined to be all in stage 1
		adderGraph << "{'A',[" << preFactor*c << "]," << coeffStage << ",[" << signAStr << preFactor*a << "]," << (preFactor*a == 1?0:1) << "," << eA << ",[" << signBStr << preFactor*b << "]," << (preFactor*b == 1?0:1) << "," << eB << "},";
	}

	void IntConstMultShiftAddOpt::buildAdderGraph(int c, int preFactor)
	{
		int i = (c-1)/2-1;

		int l1;
		int_set_t leap1_candidate_set1;
		int_set_t leap1_candidate_set2;
		int_set_t leap1_candidate_set2_div;
		int_set_t leap1_set;

		switch(solutions_gt[i])
		{
		case ADD:
			int eA,eB,signA,signB;
            REPORT(DEBUG, "additive: " << c << "=" << "Aop(" << solutions_op_a[i] << "," << solutions_op_b[i] << ")");

			assert(getExponents(solutions_op_a[i],solutions_op_b[i],c,&eA,&eB,&signA,&signB));
			generateAOp(solutions_op_a[i],solutions_op_b[i],c,eA,eB,signA,signB,preFactor);
			if(solutions_op_a[i] != 1) buildAdderGraph(solutions_op_a[i],preFactor);
			if(solutions_op_b[i] != 1) buildAdderGraph(solutions_op_b[i],preFactor);
			break;
		case MUL:
            REPORT(DEBUG, "multiplicative: " << c << " = " << solutions_op_a[i] << " * " << solutions_op_b[i]);
			preFactor *= solutions_op_b[i];
			if(solutions_op_a[i] != 1) buildAdderGraph(solutions_op_a[i],preFactor);
			if(solutions_op_b[i] != 1) buildAdderGraph(solutions_op_b[i]);
			break;
		case LF2:
            REPORT(DEBUG, "leapfrog-2: " << solutions_gt[i] << " with " << solutions_op_a[i] << " and " << solutions_op_b[i]);

			compute_successor_set(solutions_op_a[i],1,4*c,&leap1_candidate_set1,false);

			compute_successor_set(c,solutions_op_a[i],4*c,&leap1_candidate_set2,false);

			for(int_set_t::iterator it=leap1_candidate_set2.begin(); it != leap1_candidate_set2.end(); ++it)
			{
				if(*it % solutions_op_b[i] == 0)
					leap1_candidate_set2_div.insert(*it/solutions_op_b[i]);
			}

			set_intersection(leap1_candidate_set1.begin(), leap1_candidate_set1.end(), leap1_candidate_set2_div.begin(), leap1_candidate_set2_div.end(), inserter(leap1_set, leap1_set.begin()));

#ifdef DEBUG
			cout << "leap1_candidate_set1=";
			for(int_set_t::iterator it=leap1_candidate_set1.begin(); it != leap1_candidate_set1.end(); ++it)
			{
				cout << *it << " ";
			}
			cout << endl;

			cout << "leap1_candidate_set2=";
			for(int_set_t::iterator it=leap1_candidate_set2.begin(); it != leap1_candidate_set2.end(); ++it)
			{
				cout << *it << " ";
			}
			cout << endl;

			cout << "leap1_candidate_set2_div=";
			for(int_set_t::iterator it=leap1_candidate_set2_div.begin(); it != leap1_candidate_set2_div.end(); ++it)
			{
				cout << *it << " ";
			}
			cout << endl;

			cout << "leap1_set=";
			for(int_set_t::iterator it=leap1_set.begin(); it != leap1_set.end(); ++it)
			{
				cout << *it << " ";
			}
			cout << endl;
#endif //DEBUG
			l1 = *(leap1_set.begin());

			assert(getExponents(solutions_op_a[i], l1*solutions_op_b[i], c,&eA,&eB,&signA,&signB));
			generateAOp(solutions_op_a[i],l1*solutions_op_b[i],c,eA,eB,signA,signB);

			//      cout << l1*solutions_op_b[i] << " = " << l1 << " * " << solutions_op_b[i] << endl;

			assert(getExponents(1, solutions_op_a[i],l1,&eA,&eB,&signA,&signB));
			generateAOp(1,solutions_op_a[i],l1,eA,eB,signA,signB);

			if(solutions_op_a[i] != 1) buildAdderGraph(solutions_op_a[i]);
			if(solutions_op_b[i] != 1) buildAdderGraph(solutions_op_b[i],l1);
			break;
		case LF3:
			cout << "Error: Solution of " << c << " requires leapfrog3 structures which are currently unsupported (sorry)" << endl;
			exit(-1);
			break;
	  }
	}

	TestList IntConstMultShiftAddOpt::unitTest(int index)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;

		if(index==-1)
		{ // The unit tests

			vector<string> constantList; // The list of constants we want to test
//			constantList.push_back("0"); //should work in future but exceptions have to be implemented!
			constantList.push_back("1");
			constantList.push_back("16");
			constantList.push_back("123");
			constantList.push_back("3"); //1 adder
			constantList.push_back("7"); //1 subtractor
			constantList.push_back("11"); //smallest coefficient requiring 2 adders
			constantList.push_back("43"); //smallest coefficient requiring 3 adders
			constantList.push_back("683"); //smallest coefficient requiring 4 adders
			constantList.push_back("14709"); //smallest coefficient requiring 5 adders
			constantList.push_back(to_string(MAX_SCM_CONST)); //maximum supported coefficient

			for(int wIn=3; wIn<16; wIn+=4) // test various input widths
			{
				for(auto c:constantList) // test various constants
				{
					paramList.push_back(make_pair("wIn",  to_string(wIn)));
					paramList.push_back(make_pair("constant", c));
					testStateList.push_back(paramList);
					paramList.clear();
				}
			}
		}
		else
		{
			// finite number of random test computed out of index
		}

		return testStateList;
	}


    OperatorPtr flopoco::IntConstMultShiftAddOpt::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args ) {
        int wIn, constant, epsilon;

        UserInterface::parseInt( args, "wIn", &wIn );
		UserInterface::parseInt( args, "constant", &constant );
		UserInterface::parseInt( args, "epsilon", &epsilon );

        return new IntConstMultShiftAddOpt(parentOp, target, wIn, constant, false, epsilon);
    }

    void flopoco::IntConstMultShiftAddOpt::registerFactory() {

        UserInterface::add( "IntConstMultShiftAddOpt", // name
                            "Integer constant multiplication using shift and add in an optimal way (i.e., with minimum number of adders). Works for coefficients up to " + std::to_string(MAX_SCM_CONST) + " (19 bit)", // description, string
                            "ConstMultDiv", // category, from the list defined in UserInterface.cpp
                            "", //seeAlso
                            "wIn(int): Input word size; \
                            constant(int): constant; \
                            epsilon(int)=0: Allowable error for truncated constant multipliers;",
                            "Nope.",
                            IntConstMultShiftAddOpt::parseArguments,
							IntConstMultShiftAddOpt::unitTest
                          ) ;
    }

}
#else

#include "IntConstMultShiftAddOpt.hpp"
namespace flopoco
{
	void IntConstMultShiftAddOpt::registerFactory() { }
}

#endif //defined(HAVE_PAGLIB) && defined(HAVE_OSCM)

