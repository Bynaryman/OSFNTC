/*
  Integer constant multiplication using minimum number of adders due to

  Gustafsson, O., Dempster, A., Johansson, K., Macleod, M., & Wanhammar, L. (2006).
  Simplified Design of Constant Coefficient Multipliers. Circuits, Systems, and Signal Processing, 25(2), 225–251.

  All constants up to 19 bit will be realized optimal using precomputed tables provided by the SPIRAL project (http://spiral.ece.cmu.edu/mcm/).
  
  Author : Martin Kumm kumm@uni-kassel.de, (emulate adapted from Florent de Dinechin, Florent.de.Dinechin@ens-lyon.fr)

  All rights reserved.

*/
#if defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB) && defined(HAVE_SCALP)

#include <iostream>
#include <sstream>
#include <vector>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "../utils.hpp"
#include "../Operator.hpp"

#include "IntConstMultShiftAddRPAG.hpp"

#include "pagsuite/pagexponents.hpp"
#include "pagsuite/compute_successor_set.h"
#include "pagsuite/log2_64.h"
#include "pagsuite/fundamental.h"

#include <algorithm>
#include <cassert>

using namespace std;
using namespace PAGSuite;

namespace flopoco{

    IntConstMultShiftAddRPAG::IntConstMultShiftAddRPAG(Operator* parentOp, Target* target, int wIn, mpz_class coeffMpz, bool syncInOut, int epsilon)  : IntConstMultShiftAdd(parentOp, target, wIn, "", false, syncInOut, 1000, false, epsilon)
    {
		srcFileName="IntConstMultShiftAddRPAG";

		set<int_t> target_set;
		int_t coeff = mpz_get_ui(coeffMpz.get_mpz_t());

    	target_set.insert(coeff);

		int depth = log2c_64(nonzeros(coeff));

		REPORT(INFO, "depth=" << depth);

		PAGSuite::rpag *rpag = new PAGSuite::rpag(); //default is RPAG with 2 input adders

		for(int_t t : target_set)
			rpag->target_set->insert(t);

		if(depth > 3)
		{
			REPORT(DEBUG, "depth is 4 or more, limit search limit to 1");
			rpag->search_limit = 1;
		}
		if(depth > 4)
		{
			REPORT(DEBUG, "depth is 5 or more, limit MSD permutation limit");
			rpag->msd_digit_permutation_limit = 1000;
		}
		PAGSuite::global_verbose = UserInterface::verbose-1; //set rpag to one less than verbose of FloPoCo

		PAGSuite::cost_model_t cost_model = PAGSuite::LL_FPGA;// with default value
		rpag->input_wordsize = wIn;
		rpag->set_cost_model(cost_model);
		rpag->optimize();

		vector<set<int_t>> pipeline_set = rpag->get_best_pipeline_set();

		list<realization_row<int_t> > pipelined_adder_graph;
		pipeline_set_to_adder_graph(pipeline_set, pipelined_adder_graph, true, rpag->get_c_max());
		append_targets_to_adder_graph(pipeline_set, pipelined_adder_graph, target_set);

		string adderGraph = output_adder_graph(pipelined_adder_graph,true);

		REPORT(INFO, "adderGraph=" << adderGraph);

		ProcessIntConstMultShiftAdd(target,adderGraph,"",epsilon);

        ostringstream name;
        name << "IntConstMultRPAG_" << coeff << "_" << wIn;
        setName(name.str());
    }

	TestList IntConstMultShiftAddRPAG::unitTest(int index)
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
			constantList.push_back("123456789"); //test a relatively large constant

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

    OperatorPtr flopoco::IntConstMultShiftAddRPAG::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args ) {
        int wIn, epsilon;

        UserInterface::parseStrictlyPositiveInt( args, "wIn", &wIn );
		UserInterface::parsePositiveInt( args, "epsilon", &epsilon );

		string	constStr;
		UserInterface::parseString(args, "constant", &constStr);
		try {
			mpz_class constant(constStr);
			return new IntConstMultShiftAddRPAG(parentOp, target, wIn, constant, false, epsilon);
		}
		catch (const std::invalid_argument &e) {
			cerr << "Error: Invalid constant " << constStr <<endl;
			exit(EXIT_FAILURE);
		}

    }

    void flopoco::IntConstMultShiftAddRPAG::registerFactory() {

        UserInterface::add( "IntConstMultShiftAddRPAG", // name
                            "Integer constant multiplication using shift and add using the RPAG algorithm", // description, string
                            "ConstMultDiv", // category, from the list defined in UserInterface.cpp
                            "", //seeAlso
                            "wIn(int): Input word size; \
                            constant(int): constant; \
                            epsilon(int)=0: Allowable error for truncated constant multipliers;",
                            "Nope.",
                            IntConstMultShiftAddRPAG::parseArguments,
							IntConstMultShiftAddRPAG::unitTest
                          ) ;
    }

}
#else

#include "IntConstMultShiftAddRPAG.hpp"
namespace flopoco
{
	void IntConstMultShiftAddRPAG::registerFactory() { }
}

#endif //defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB)

