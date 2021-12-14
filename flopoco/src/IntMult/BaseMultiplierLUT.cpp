#include "BaseMultiplierLUT.hpp"
#include "IntMultiplierLUT.hpp"

namespace flopoco {

Operator* BaseMultiplierLUT::generateOperator(
		Operator *parentOp, 
		Target* target,
		Parametrization const & parameters) const
{
	return new IntMultiplierLUT(
			parentOp,
			target,
			parameters.getMultXWordSize(),
			parameters.getMultYWordSize(),
			parameters.isSignedMultX() and parameters.getMultXWordSize() > 1,
			parameters.isSignedMultY() and parameters.getMultYWordSize() > 1,
			parameters.isFlippedXY()
		);
}

    double BaseMultiplierLUT::getLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY){
        int ws = (wX()==1)?wY():((wY()==1)?wX():wX()+wY());
        int luts = ((ws <= 5)?ws/2+ws%2:ws);

        int x_min = ((x_anchor < 0)?0: x_anchor);
        int y_min = ((y_anchor < 0)?0: y_anchor);
        int lsb = x_min + y_min;

        int x_max = ((wMultX < x_anchor + (int)wX())?wMultX: x_anchor + wX());
        int y_max = ((wMultY < y_anchor + (int)wY())?wMultY: y_anchor + wY());
        int msb = x_max+y_max;
        ws = (x_max-x_min==1)?y_max-y_min:((y_max-y_min==1)?x_max-x_min:msb - lsb);

        return luts + ws*0.65;
    }

    int BaseMultiplierLUT::ownLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY) {
        int ws = (wX()==1)?wY():((wY()==1)?wX():wX()+wY());
        int luts = ((ws <= 5)?ws/2+ws%2:ws);
        return luts;
    }


}   //end namespace flopoco

