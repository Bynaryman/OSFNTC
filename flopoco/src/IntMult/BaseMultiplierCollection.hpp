#ifndef BaseMultiplierCollection_HPP
#define BaseMultiplierCollection_HPP

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "BaseMultiplierCategory.hpp"
#include "Target.hpp"

namespace flopoco {
	typedef size_t base_multiplier_id_t;
    class BaseMultiplierCollection {

	public:
		BaseMultiplierCollection(Target *target);

        BaseMultiplierCategory& getBaseMultiplier(base_multiplier_id_t multRef);
		base_multiplier_id_t getPreferedMultiplier(); //remove ?

        string getName(){ return uniqueName_; }

        unsigned size() { return baseMultiplierCategories.size(); }

		void print();

		Target* getTarget() { return target; }
	private:
        Target* target;

        string srcFileName; //for debug outputs
        string uniqueName_; /**< useful only to enable same kind of reporting as for FloPoCo operators. */

        vector<BaseMultiplierCategory*> baseMultiplierCategories;
	};
}
#endif
