#ifndef PIFADD_HPP
#define PIFADD_HPP
#include "../../Operator.hpp"
#include "../../utils.hpp"

namespace flopoco{
  class PIFAdd : public Operator {
	public:
	        PIFAdd(Target* target, Operator* parentOp, int wE, int wF);
	  
		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();

		/** emulate() function to be shared by various implementations */
		void emulate(TestCase * tc);

	private:
	  int wE_;
	  int wF_;

	};
}
#endif
