#ifndef POSITADD_HPP
#define POSITADD_HPP
#include "../../Operator.hpp"
#include "../../utils.hpp"

namespace flopoco{
  class PositAdd : public Operator {
	public:
	        PositAdd(Target* target, Operator* parentOp, int width, int wES);
	  
		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();

		/** emulate() function to be shared by various implementations */
		void emulate(TestCase * tc);

	private:
          int width_;
          int wES_;
	  int wE_;
	  int wF_;

	};
}
#endif
