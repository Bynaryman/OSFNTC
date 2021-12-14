#ifndef FIXREALCONSTMULT_HPP
#define FIXREALCONSTMULT_HPP

#include "../Operator.hpp"

namespace flopoco
{
class FixRealConstMult : public Operator
{
public:
	enum Method {automatic, KCM, ShiftAdd};

	/**
	 * @brief Faithful multiplication of a fixed point number by a real constant.
     *        Internally it is realized as a wrapper for FixRealKCM and FixRealShiftAdd.
	 * @param target : target on which we want the KCM to run
	 * @param signedIn : true if input are 2'complement fixed point numbers
	 * @param msbin : power of two associated with input msb. For unsigned
	 * 				  input, msb weight will be 2^msb, for signed input, it
	 * 				  will be -2^msb
	 * 	@param lsbIn :  Weight of the least significant bit of the input
	 * 	@param lsbOut : Weight of the least significant bit of the output
	 * 	@param constant : string that describes the constant in sollya syntax
	 * 	@param targetUlpError :  error bound on result. Difference
	 * 							between result and real value should be
	 * 							less than targetUlpError * 2^lsbOut.
	 * 							Value has to be in ]0.5 ; 1] (if 0.5 wanted,
	 * 							please consider to create a one bit more
	 * 							precise KCM with a targetUlpError of 1 and
	 * 							truncate the result
	 */
	FixRealConstMult(OperatorPtr parentOp, Target *target, bool signedIn_, int msbIn_, int lsbIn_, int lsbOut_, string constant_, double targetUlpError_, FixRealConstMult::Method method_);

	/* This constructor is just for initialization
	 * */
	FixRealConstMult(OperatorPtr parentOp, Target *target, bool signedIn_, int msbIn_, int lsbIn_, int lsbOut_, string constant_, double targetUlpError_);

	/* This constructor is required for the virtual KCM operator and just for initialization
	 * */
	FixRealConstMult(Operator* parentOp, Target* target);

	static OperatorPtr parseArguments(OperatorPtr parentOp, Target* target, vector<string>& args			);
	static void registerFactory();

	void emulate(TestCase* tc);
	static TestList unitTest(int index);

protected:
	bool signedIn;
	int msbIn;
	int lsbIn;
	int msbOut;
	int lsbOut;
	string constant;
	float targetUlpError;

	int msbC;

	mpfr_t mpC;
	mpfr_t absC;
	bool negativeConstant;
	bool signedOutput; /**< computed: true if the constant is negative or the input is signed */

	void constStringToSollya();
};

}

#endif //FIXREALCONSTMULT_HPP
