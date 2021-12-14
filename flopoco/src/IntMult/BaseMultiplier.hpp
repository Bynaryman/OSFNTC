#ifndef BaseMultiplier_HPP
#define BaseMultiplier_HPP

#include <string>
#include <iostream>
#include <string>
#include "Target.hpp"
#include "Operator.hpp"

namespace flopoco {
    class BaseMultiplier {
	protected:
		struct BaseMultiplierParametrization;
	public:
		typedef BaseMultiplierParametrization base_multiplier_parametrization_t;	
        BaseMultiplier(bool isSignedX, bool isSignedY);

        virtual ~BaseMultiplier();

        int getXWordSize() const { return wX; }
        int getYWordSize() const { return wY; }
        int getOutWordSize() const { return wR; }

        /**
         * @brief generateOperator generates an instance of the corresponding Operator that realizes the given shape
         * @return the generated operator
         */
		virtual Operator *generateOperator(Operator *parentOp, Target* target) = 0;

        /**
         * @brief Returns true if x and y coordinates are at valid shape positions, override this function if shape is non rectangular within wX and wY
         * @param x: x-coordinate (relative to multiplier coordinate)
         * @param y: y-coordinate (relative to multiplier coordinate)
         * @return true if x and y coordinates are at valid shape positions, false otherwise
         */
        virtual bool shapeValid(int x, int y);

        string getName(){ return uniqueName_; }

    protected:

        int wX, wY, wR;

        bool isSignedX, isSignedY;

        string srcFileName; //for debug outputs

        string uniqueName_; /**< useful only to enable same kind of reporting as for FloPoCo operators. */

		struct BaseMultiplierParametrization {
			int wX_;
			int wY_;
			bool isFlippedXY;
			bool signedX;
			bool signedY;
		};
	
	};
}
#endif
