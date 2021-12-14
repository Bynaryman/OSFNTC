#ifndef FIXFIR_HPP
#define FIXFIR_HPP

#include "Operator.hpp"
#include "FixSOPC.hpp"
#include "utils.hpp"

namespace flopoco{


	class FixFIR : public Operator {

	public:
		
		/**
		 * @brief 				constructor, building the FIR out of the coefficients, with (a possibly different) lsbIn and lsbOut.
		 * @param coeff			The coefficients are considered as real numbers, provided as string expresssions such as 0.1564565756768 or sin(3*pi/8)
		 * Input is assumed to be in (-1,1) with lsb at postion lsbIn.
		 * @param lsbIn			Input has lsb at position lsbIn.
		 * @param lsbOut		Output has lsb at position lsbOut.
		 * @param symmetry	0 for normal filter, 1 if the coefficients are symmetric, -1 if they are antisymmetric. We could probably attempt to detect it 
		 * @param rescale
		 *						If rescale=false, the msb of the output is computed so as to avoid overflow.
		 *						If rescale=true, all the coefficients are rescaled by 1/sum(|coeffs|).
		 * This way the output is also in [-1,1], output size is equal to input size, and the output signal makes full use of the output range.
		*/
		FixFIR(OperatorPtr parentOp, Target* target, int lsbIn, int lsbOut, vector<string> coeff, int symmetry=0, bool rescale=false);

		/**
		 * @brief 				empty constructor, to be called by subclasses.
		*/
		FixFIR(OperatorPtr parentOp, Target* target, int lsbIn, int lsbOut);

		/** @brief Destructor */
		~FixFIR();

		// Below all the functions needed to test the operator
		/**
		 * @brief the emulate function is used to simulate in software the operator
		 * in order to compare this result with those outputed by the vhdl opertator
		 */
		void emulate(TestCase * tc);

		// User-interface stuff
		/** Factory method */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
		static void registerFactory();

	protected:
		/**
		 * @brief The method that does the bulk of operator construction, isolated to enable sub-classes such as FixHalfSine etc
		 */
		void buildVHDL();


		/**
		 * @brief The method that does the common initialization work between the constructors
		 */
		void initFilter();

		/**
		 * @brief The method that does the bulk of operator construction, isolated to enable sub-classes such as FixHalfSine etc
		 * 		  The case when the filter is symmetric
		 */
		void buildVHDLSymmetric();

		int n;								/**< number of taps */
		int lsbIn;							/**< lsbIn of the filter */
		int lsbOut;							/**< lsbOut of the filter */
		int msbOut;							/**< msbOut of the filter, if different from lsbIn */
		vector<string> coeff;			  	/**< the coefficients as strings */
		vector<string> coeffSymmetric;	  	/**< the coefficients as strings, in case of a symmetric filter */
		int symmetry;					/**< flag that shows if the filter is implemented as a symmetric filter */
		bool rescale; 						/**< if true, the output is rescaled to [-1,1]  (to the same format as input) */
	private:
		mpz_class xHistory[10000]; 			// history of x used by emulate
		int currentIndex;
		FixSOPC *fixSOPC; 					/**< the SOPC used for VHDL generation  */
		FixSOPC *refFixSOPC;				/**< usually equal to fixSOPC, except in the case of a symmetric filter, where it is a virtual, nave SOPC that is used only in emulate() */
		vector<double> coeffD;	  	/**< the coefficients rounded to doubles, used for symmetry checks */
	};

}

#endif
