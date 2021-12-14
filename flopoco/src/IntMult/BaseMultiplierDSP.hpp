#ifndef BaseMultiplierDSP_HPP
#define BaseMultiplierDSP_HPP

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


    class BaseMultiplierDSP : public BaseMultiplierCategory
    {

	public:
		BaseMultiplierDSP(
				int maxWidthBiggestWord,
				int maxWidthSmallestWord,
				int deltaWidthSigned
			) : BaseMultiplierCategory{
				maxWidthBiggestWord,
				maxWidthSmallestWord,
				false,
                false,
				-1,
				"BaseMultiplierDSP"
		}{}

        BaseMultiplierDSP(
                int wX,
                int wY,
                bool isSignedX,
                bool isSignedY
        ) : BaseMultiplierCategory{
                wX,
                wY,
                isSignedX,
                isSignedY,
                -1,
                "BaseMultiplierDSP"
        }{}


        int getDSPCost() const final {return 1;}

		Operator* generateOperator(
				Operator *parentOp,
				Target *target,
				Parametrization const & parameters
			) const final;

    private:
        bool flipXY;
        bool pipelineDSPs;
	};
}
#endif
