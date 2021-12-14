#ifndef BaseMultiplierLUT_HPP
#define BaseMultiplierLUT_HPP

#include <string>
#include <iostream>
#include <string>
#include <gmp.h>
#include <gmpxx.h>
#include "Target.hpp"
#include "Operator.hpp"
#include "BaseMultiplierCategory.hpp"

namespace flopoco {
    class BaseMultiplierLUT : public BaseMultiplierCategory
    {

	public:
        BaseMultiplierLUT(int maxSize, double lutPerOutputBit):
                BaseMultiplierCategory{
                        maxSize,
                        maxSize,
                        false,
                        false,
                        -1,
                        "BaseMultiplierLUT"
                }, lutPerOutputBit_{lutPerOutputBit}
        {}

        BaseMultiplierLUT(
                int wX,
                int wY
        ) : BaseMultiplierCategory{
                    wX,
                    wY,
                    false,
                    false,
                    -1,
                    "BaseMultiplierLUT_" + string(1,((char) wX) + '0') + "x" + string(1,((char) wY) + '0')
        }{}

            int getDSPCost() const override { return 0; }
            double getLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY);
            int ownLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY);

			Operator *generateOperator(
					Operator *parentOp,
					Target *target,
					Parametrization const & parameters
				) const final;
	private:
			double lutPerOutputBit_;
	};
}
#endif
