#ifndef FixAtan2ByCORDIC_HPP
#define FixAtan2ByCORDIC_HPP

#include "FixAtan2.hpp"
#include "utils.hpp"


#include <vector>

namespace flopoco{ 

	
	class FixAtan2ByCORDIC : public FixAtan2 {
	  
	  public:

		// Possible TODO: add an option to obtain angles in radians or whatever

		/** Constructor: w is the input and output size, all signed fixed-point number. 
		 Angle is output as a signed number between 00...00 and 11...1111, for 2pi(1-2^-w)
		      pi is 0100..00, etc.
		Actual position of the fixed point in the inputs doesn't matter as long as it is the same for x and y

		*/
		FixAtan2ByCORDIC(Target* target, int wIn, int wOut, map<string, double> inputDelays = emptyDelayMap);

		// destructor
		~FixAtan2ByCORDIC();
		


	private:
		int	maxIterations;         /**< index at which iterations stop */
		int gXY;                   /**< number of guard bits on the (X,Y) datapath */
		int gA;                    /**< number of guard bits on the Angle datapath */
		vector<mpfr_t> atani;      /**< */

		void computeGuardBits();
		
	};

}

#endif
