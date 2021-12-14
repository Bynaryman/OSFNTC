#include <algorithm>
#include <numeric>

#include "BaseMultiplierCollection.hpp"
#include "BaseMultiplierLUT.hpp"
#include "BaseMultiplierXilinx2xk.hpp"
#include "BaseMultiplierDSP.hpp"
#include "BaseMultiplierDSPSuperTilesXilinx.hpp"
#include "TargetBaseMultSpecialisation.hpp"

using namespace std;

namespace flopoco {
base_multiplier_id_t BaseMultiplierCollection::getPreferedMultiplier()
{
	return 0;
}

BaseMultiplierCollection::BaseMultiplierCollection(Target* target) : target(target)
{
    srcFileName = "BaseMultiplierCollection";
    uniqueName_ = "BaseMultiplierCollection";

	//DSP based multiplier
	int wX, wY, wXSigned, wYSigned;
	target->getMaxDSPWidths(wX, wY);
	target->getMaxDSPWidths(wXSigned, wYSigned, true);
	int deltaSigned = wXSigned - wX;

	int maxW = max(wX, wY);
	int minW = min(wX, wY);

	baseMultiplierCategories.push_back(
			new BaseMultiplierDSP(maxW, minW, deltaSigned)
		);

	//LUT based multipliers
	double score = target->lutConsumption(1);
	for (int i = 1 ; i <= target->maxLutInputs(); i++) {
		double newScore = target->lutConsumption(i+1);
		if (newScore != score) {
			baseMultiplierCategories.push_back(new BaseMultiplierLUT(i, score));
			score = newScore;
		}
	}

	if(target->useTargetOptimizations())
	{
		for (BaseMultiplierCategory* t : TargetSpecificBaseMultiplier(target)) {
			baseMultiplierCategories.push_back(t);
		}
	}
}

BaseMultiplierCategory& BaseMultiplierCollection::getBaseMultiplier(
		base_multiplier_id_t multRef
	)
{
    if(multRef < baseMultiplierCategories.size())
	{
        return *(baseMultiplierCategories[multRef]);
	}
	else {
		throw string("BaseMultiplierCollection::getBaseMultiplier: Invalid base multiplier index");
	}
}

/*
vector<BaseMultiplierCategory const *> const BaseMultiplierCollection::getView() const
{
	vector<BaseMultiplierCategory const *> ret;
	copy(
			baseMultiplierCategories.begin(),
			baseMultiplierCategories.end(),
			ret.begin()
		);
	return ret;
}
*/

void BaseMultiplierCollection::print()
{
	if (UserInterface::verbose >= DETAILED)
	{
		REPORT(DETAILED, "Available base multipliers:");
		for(BaseMultiplierCategory *bmc : baseMultiplierCategories)
		{
			cerr << "  base multiplier " << bmc->getMaxWordSizeSmallInputUnsigned() << "x" << bmc->getMaxWordSizeLargeInputUnsigned()
				 << ", max. DSP cost: " << bmc->getDSPCost()
				 << ", max. LUT cost: " << bmc->getLUTCost(0,0, 48, 48) << endl;
		}

	}
}

}   //end namespace flopoco

