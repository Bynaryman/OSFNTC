#ifndef Fix2DNorm_HPP
#define Fix2DNorm_HPP

#include "FixAtan2.hpp"
#include "utils.hpp"


#include <vector>

namespace flopoco{ 

	
	class Fix2DNorm: public Operator {
	  
	  public:


		/** Constructor:  all unsigned fixed-point number. 

		*/
		Fix2DNorm(Target* target, int msb, int lsb);

		// destructor
		~Fix2DNorm();
		
		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();


	private:
	int msb; /**< input/output msb */
	int lsb; /**< input/output lsb */
		
	};
}

#endif
