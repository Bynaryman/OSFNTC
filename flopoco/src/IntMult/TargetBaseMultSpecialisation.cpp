#include "TargetBaseMultSpecialisation.hpp"
#include "Targets/Kintex7.hpp"
#include "Targets/Virtex6.hpp"
#include "BaseMultiplierXilinx2xk.hpp"
#include "BaseMultiplierDSPSuperTilesXilinx.hpp"
#include "BaseMultiplierIrregularLUTXilinx.hpp"

using namespace std;

namespace flopoco{
	/*
	 * @param bmVec the vector to populate with the target specific
	 * baseMultipliers
	 */
	vector<BaseMultiplierCategory*> TargetSpecificBaseMultiplier(
			Target const* target
		)
	{
		vector<BaseMultiplierCategory*> ret;
		if (typeid(*target) == typeid(Virtex6) || typeid(*target) == typeid(Kintex7)) {
			ret.push_back(new BaseMultiplierXilinx2xk(128,2));
		}
		return ret;
	}
}
