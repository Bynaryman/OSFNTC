#include "Operator.hpp"

namespace flopoco
{

class IntMultiplierLUT : public Operator
{
public:
	IntMultiplierLUT(Operator *parentOp, Target* target, int wX, int wY, bool isSignedX=false, bool isSignedY=false, bool flipXY=false);

	/** Factory method */
	static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
	/** Register the factory */
	static void registerFactory();

	void emulate(TestCase* tc);
	static TestList unitTest(int index);

private:
	int wX, wY;
	bool isSignedX, isSignedY;
	mpz_class function(int x);
};

}
