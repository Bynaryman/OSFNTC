/*
  Integer constant multiplication using minimum number of ternary adders due to

  Kumm, M., Gustafsson, O., Garrido, M., & Zipf, P. (2018). Optimal Single Constant Multiplication using Ternary Adders.
  IEEE Transactions on Circuits and Systems II: Express Briefs, 65(7), 928–932. http://doi.org/10.1109/TCSII.2016.2631630

  Author : Martin Kumm kumm@uni-kassel.de, (emulate adapted from Florent de Dinechin, Florent.de.Dinechin@ens-lyon.fr)

  All rights reserved.

*/
#if defined(HAVE_PAGLIB)

#include <iostream>
#include <sstream>
#include <vector>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "../utils.hpp"
#include "../Operator.hpp"

#include "IntConstMultShiftAddOptTernary.hpp"
#include "IntConstMultShiftAdd.hpp"

#include "tscm_solutions.hpp"
#include "pagsuite/pagexponents.hpp"

#include "pagsuite/compute_successor_set.h"
#include "pagsuite/log2_64.h"
#include "pagsuite/fundamental.h"

#include <algorithm>

using namespace std;
using namespace PAGSuite;

namespace flopoco{

    IntConstMultShiftAddOptTernary::IntConstMultShiftAddOptTernary(Operator* parentOp, Target* target, int wIn, int coeff, bool syncInOut) : IntConstMultShiftAdd(parentOp, target, wIn, "", false, syncInOut, 1000, false, epsilon)
    {
		int maxCoefficient = 4194303; //=2^22-1

        int shift;
        int coeffOdd = fundamental(coeff, &shift);

		if(coeffOdd <= maxCoefficient)
        {
            cout << "nofs[" << coeff << "]=" << nofs[(coeff-1)>>1][0] << " " << nofs[(coeff-1)>>1][1] << endl;

            int nof1 = nofs[(coeffOdd-1)>>1][0];
            int nof2 = nofs[(coeffOdd-1)>>1][1];

            set<int> coefficient_set;
            coefficient_set.insert(coeff);

            set<int> nof_set;
            if(nof1 != 0) nof_set.insert(nof1);
            if(nof2 != 0) nof_set.insert(nof2);

            string adderGraphString;

            computeAdderGraphTernary(nof_set, coefficient_set, adderGraphString);

            stringstream adderGraphStringStream;
            adderGraphStringStream << "{" << adderGraphString;

            if(coeff != coeffOdd)
            {
                if(adderGraphString.length() > 1) adderGraphStringStream << ",";
				adderGraphStringStream << "{'O',[" << coeff << "],2,[" << coeffOdd << "],1," << shift << "}";
            }
            adderGraphStringStream << "}";

            cout << "adder_graph=" << adderGraphStringStream.str() << endl;

            ProcessIntConstMultShiftAdd(target,adderGraphStringStream.str());

            ostringstream name;
            name << "IntConstMultShiftAddOptTernary_" << coeffOdd << "_" << wIn;
            setName(name.str());

        }
        else
        {
			cerr << "Error: Coefficient too large, max. odd coefficient is " << maxCoefficient << endl;
            exit(-1);
        }

	}

    OperatorPtr flopoco::IntConstMultShiftAddOptTernary::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args ) {
        int wIn, constant;

        UserInterface::parseInt( args, "wIn", &wIn );
        UserInterface::parseInt( args, "constant", &constant );

        return new IntConstMultShiftAddOptTernary(parentOp, target, wIn, constant, false);
    }
}
#endif

namespace flopoco{
    void flopoco::IntConstMultShiftAddOptTernary::registerFactory() {

#if defined(HAVE_PAGLIB)
        UserInterface::add( "IntConstMultShiftAddOptTernary", // name
                            "Integer constant multiplication using shift and ternary additions in an optimal way (i.e., with minimum number of ternary adders). Works for coefficients up to 4194303 (22 bit)", // description, string
                            "ConstMultDiv", // category, from the list defined in UserInterface.cpp
                            "", //seeAlso
                            "wIn(int): Input word size; \
                            constant(int): constant;",
                            "Nope.",
                            IntConstMultShiftAddOptTernary::parseArguments
                          ) ;
#endif
    }
}

