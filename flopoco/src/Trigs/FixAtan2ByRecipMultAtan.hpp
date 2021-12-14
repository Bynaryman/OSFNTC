#ifndef FixAtan2ByRecipMultAtan_HPP
#define FixAtan2ByRecipMultAtan_HPP

#include "FixAtan2.hpp"
#include "utils.hpp"


#include <vector>

namespace flopoco{ 

	
	class FixAtan2ByRecipMultAtan : public FixAtan2 {
	  
	  public:

		// Possible TODO: add an option to obtain angles in radians or whatever

		/** Constructor: w is the input and output size, all signed fixed-point number. 
		 Angle is output as a signed number between 00...00 and 11...1111, for 2pi(1-2^-w)
		      pi is 0100..00, etc.
		Actual position of the fixed point in the inputs doesn't matter as long as it is the same for x and y

		*/
		FixAtan2ByRecipMultAtan(Target* target, int wIn, int wOut, int method=0, map<string, double> inputDelays = emptyDelayMap);

		// destructor
		~FixAtan2ByRecipMultAtan();
		


	private:
		
	};

}

#endif
