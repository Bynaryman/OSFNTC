#ifndef FixComplexAdder_HPP
#define FixComplexAdder_HPP
#include <vector>
#include <sstream>

#include "Operator.hpp"
#include "utils.hpp"

namespace flopoco{

	/**
	 * @brief Complex adder for fixed point numbers
	 */
	class FixComplexAdder : public Operator
	{
	public:
		FixComplexAdder(OperatorPtr parentOp, Target* target, int msbin, int lsbin, int msbout, int lsbout, bool signedIn = true, bool addorsub = true, bool laststage = false);
		~FixComplexAdder();

		void emulate(TestCase * tc);

		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
		static void registerFactory();

private:
		int msbin;					/**< weight of the MSB in the input, considered as a signed number in (-1,1) */
		int lsbin;					/**< weight of the LSB in the input, considered as a signed number in (-1,1) */
		int msbout;					/**< weight of the MSB in the result */
		int lsbout;					/**< weight of the LSB in the result */
		bool signedIn;
		bool addorsub;
		bool laststage;

	};
}
#endif
