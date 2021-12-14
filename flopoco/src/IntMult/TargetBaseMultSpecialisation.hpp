#ifndef TARGETBASEMULTSPECIALISATION_HPP
#define TARGETBASEMULTSPECIALISATION_HPP

#include <typeinfo>
#include <vector>

#include "BaseMultiplierCategory.hpp"
#include "Target.hpp"

namespace flopoco{

	/**
	 * @brief Add the BaseMultiplierCategory specific to a certain target to the 
	 * 		  basemultiplierCategory vector
	 */
	vector<BaseMultiplierCategory*> TargetSpecificBaseMultiplier(Target const * target);
}
#endif
