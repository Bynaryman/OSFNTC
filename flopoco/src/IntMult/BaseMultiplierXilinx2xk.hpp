#ifndef XilinxXilinxBaseMultiplier2xk_HPP
#define XilinxXilinxBaseMultiplier2xk_HPP

#include <string>
#include <iostream>
#include <string>
#include <gmp.h>
#include <gmpxx.h>
#include "Target.hpp"
#include "Operator.hpp"
#include "Table.hpp"
#include "BaseMultiplierCategory.hpp"

namespace flopoco {
    class BaseMultiplierXilinx2xk : public BaseMultiplierCategory
    {
	public:
        BaseMultiplierXilinx2xk(int wX, int wY):
			BaseMultiplierCategory{
            wX,
            wY,
            false,
            false,
            -1,
            "BaseMultiplierXilinx2xk_" + to_string(wX) + "x" +  to_string(wY)}
		{}

        int getDSPCost() const override { return 0; }
        bool isVariable() const override { return true; }
        double getLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY);
        int ownLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY);

		/** Factory method */
        static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
        /** Register the factory */
        static void registerFactory();
        static TestList unitTest(int index);

		Operator *generateOperator(
				Operator *parentOp,
				Target *target,
				Parametrization const & parameters
			) const final;
	};

    class BaseMultiplierXilinx2xkOp : public Operator
    {
    public:
		BaseMultiplierXilinx2xkOp(Operator *parentOp, Target* target, bool isSignedX, bool isSignedY, int wX, int wY);

        void emulate(TestCase* tc);
    private:
        int wX, wY;
    };

}
#endif
