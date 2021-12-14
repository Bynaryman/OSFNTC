#ifndef _BASICPOLYAPPROX_HPP_
#define _BASICPOLYAPPROX_HPP_

#include <string>
#include <iostream>

#include <sollya.h>
#include <gmpxx.h>

#include "../Operator.hpp" // mostly for reporting
#include "../UserInterface.hpp"
#include "FixFunction.hpp"
#include "FixConstant.hpp"

using namespace std;

/* Stylistic convention here: all the sollya_obj_t have names that end with a capital S */
namespace flopoco{

	/** The BasicPolyApprox object builds and maintains a machine-efficient polynomial approximation to a fixed-point function over [0,1]
			Fixed point, hence only absolute errors/accuracy targets.

			There are two families of constructors:
			one that inputs target accuracy and computes degree and approximation error (calling the buildApproxFromTargetAccuracy method)
			one that inputs degree and computes approximation error (calling the buildApproxFromDegreeAndLSBs method).

			The first is useful in standalone, or to evaluate degree/MSBs etc.
			The second is needed in the typical case of a domain split, where the degree is determined when determining the split.

			Sketch of the algorithm for  buildApproxFromTargetAccuracy:
				guessDegree gives a tentative degree.
				target_accuracy defines the best-case LSB of the constant part of the polynomial.
				if  addGuardBitsToConstant, we add g=ceil(log2(degree+1)) bits to the LSB of the constant:
				this provides a bit of freedom to fpminimax, for free in terms of evaluation.
				And we call fpminimax with that, and check what it provided.
				Since x is in [0,1], the MSB of coefficient i is then exactly the MSB of a_i.x^i
				For the same reason, all the coefficients should have the same target LSB

			No domain splitting or other range reduction here: these should be managed in different classes
			No good handling of functions with zero coefficients for now.
			- a function with zero constant should be transformed into a "proper" one outside this class.
			   Example: sin, log(1+x)
		  - a function with odd or even Taylor should be transformed as per the Muller book.

			To implement a generic approximator we will need to lift these restrictions, but it is unclear that it needs to be in this class.
			A short term TODO is to detect such cases.
	*/

	class BasicPolyApprox {
	public:


		/** A minimal constructor inputting target accuracy.
				@param f: defines the function and if the input interval is [0,1] or [-1,1]
				@param targetAccuracy the target accuracy
				@param addGuardBits:
				if >=0, add this number of bits to the LSB of each coeff
				if -1, add to each coeff a number of LSB bits that corresponds to the bits needed for a faithful Horner evaluation based on faithful (truncated) multipliers
		 */
		BasicPolyApprox(FixFunction* f, double targetAccuracy, int addGuardBits=-1);

		/** A minimal constructor that inputs a sollya_obj_t function and the target accuracy
				@param fS: defines the function, as a sollya_obj_t
				@param targetAccuracy the target accuracy
				@param addGuardBits:
				if >=0, add this number of bits to the LSB of each coeff
				if -1, add to each coeff a number of LSB bits that corresponds to the bits needed for a faithful Horner evaluation based on faithful (truncated) multipliers
				@param signedIn:  if true, we consider an approximation on [-1,1]. If false, it will be on [0,1]
		 */
		BasicPolyApprox(sollya_obj_t fS, double targetAccuracy, int addGuardBits=-1, bool signedIn =false);


		/** A minimal constructor that inputs a sollya_obj_t function, a degree and the weight of the LSBs.
				This one is mostly for "internal" use by classes that compute the degree separately, e.g. PiecewisePolyApprox
				@param fS: defines the function, as a sollya_obj_t
				@param degree: degree of the polynomial
				@param LSB: weight of the coefficients
				@param signedIn:  if true, we consider an approximation on [-1,1]. If false, it will be on [0,1]

		 */
		BasicPolyApprox(sollya_obj_t fS, int degree, int LSB, bool signedIn =false);

		/** A minimal constructor that parses a sollya string, inputting target accuracy
				@param sollyaString: defines the function, as a sollya parsable string
				@param targetAccuracy the target accuracy
				@param addGuardBits:
				if >=0, add this number of bits to the LSB of each coeff
				if -1, add to each coeff a number of LSB bits that corresponds to the bits needed for a faithful Horner evaluation based on faithful (truncated) multipliers
				@param signedIn:  if true, we consider an approximation on [-1,1]. If false, it will be on [0,1]
		 */
		BasicPolyApprox(string sollyaString, double targetAccuracy, int addGuardBits=-1, bool signedIn=false);

		/** A constructor for the case you already have the coefficients, e.g. you read them from a file. Beware, f is un-initialized in this case
		 * 		@param degree: degree of the polynomial
		 * 		@param MSBs: vector of the MSBs of the coefficients
				@param LSB: weight of the coefficients
				@param coeffs: the coefficients of the approximating polynomial
		 */
		BasicPolyApprox(int degree, vector<int> MSB, int LSB, vector<mpz_class> coeff);


		virtual ~BasicPolyApprox();


		/** A wrapper for Sollya guessdegree
		 */
		static	void guessDegree(sollya_obj_t fS, sollya_obj_t rangeS, double targetAccuracy, int* degreeInfP, int* degreeSupP);


		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args);

		static void registerFactory();


		vector<FixConstant*> coeff;       /**< polynomial coefficients in a hardware-ready form */
		int degree;                       /**< degree of the polynomial approximation */
		double approxErrorBound;          /**< guaranteed upper bound on the approx error of the approximation provided. Should be smaller than targetAccuracy */
		int LSB;                          /**< weight of the LSB of the polynomial approximation. Also weight of the LSB of each constant, since x \in [0,1) */
		FixFunction *f;                   /**< The function to be approximated */

	private:
		/** initialization of various constant objects for Sollya
		 * */
		void initialize();

		/** constructor code for the general case factored out
		 * */
		void buildApproxFromTargetAccuracy(double targetAccuracy, int addGuardBitsToConstant);

		/** build an approximation of a certain degree, LSB being already defined, then computes the approx error.
				Essentially a wrapper for Sollya fpminimax() followed by supnorm()
		*/
		void buildApproxFromDegreeAndLSBs();

		/** Build coeff, the vector of coefficients, out of polynomialS, the sollya polynomial
		 	 Constructor code, factored out
		 * */
		void buildFixFormatVector();


		sollya_obj_t polynomialS;         /**< The polynomial approximating it */

		string srcFileName;               /**< useful only to enable same kind of reporting as for FloPoCo operators. */
		string uniqueName_;               /**< useful only to enable same kind of reporting as for FloPoCo operators. */
		bool needToFreeF;                 /**< in an ideal world, this should not exist */

		sollya_obj_t fixedS;              /**< a constant sollya_obj_t, which indicates that fixed-point formats should be used for fpminimax */
		sollya_obj_t absoluteS;           /**< a constant sollya_obj_t, which indicates that fpminimax must try to minimize the absolute error */

	};

}
#endif // _POLYAPPROX_HH_


// Garbage below
