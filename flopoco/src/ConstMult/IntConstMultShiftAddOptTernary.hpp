/*
  Integer constant multiplication using minimum number of ternary adders due to

  Kumm, M., Gustafsson, O., Garrido, M., & Zipf, P. (2018). Optimal Single Constant Multiplication using Ternary Adders.
  IEEE Transactions on Circuits and Systems II: Express Briefs, 65(7), 928–932. http://doi.org/10.1109/TCSII.2016.2631630

  Author : Martin Kumm kumm@uni-kassel.de, (emulate adapted from Florent de Dinechin, Florent.de.Dinechin@ens-lyon.fr)

  All rights reserved.

*/
#ifndef IntConstMultShiftAddOptTernary_HPP
#define IntConstMultShiftAddOptTernary_HPP

#if defined(HAVE_PAGLIB)

#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include <cstdlib>

#include "../Operator.hpp"
#include "IntAddSubCmp/IntAdder.hpp"
#include "IntConstMultShiftAdd.hpp"

namespace flopoco{

    class IntConstMultShiftAddOptTernary : public IntConstMultShiftAdd
	{
	public:
        IntConstMultShiftAddOptTernary(Operator* parentOp, Target* target, int wIn, int coeff, bool syncInOut=true);

        static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args );
        static void registerFactory();

	private:
		int coeff;  /**< The constant */

	};
}

#endif //HAVE_PAGLIB
#endif
