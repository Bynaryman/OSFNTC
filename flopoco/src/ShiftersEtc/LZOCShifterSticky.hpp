#ifndef LZOCShifterSticky_HPP
#define LZOCShifterSticky_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "utils.hpp"
#include "Operator.hpp"

namespace flopoco{

	/** 
	 * A leading zero/one counter + shifter + sticky bit computer for FloPoCo
	 */ 
	class LZOCShifterSticky : public Operator
	{
	public:
	
		/** 
		 *  LZOCShifterSticky constructor, used in FPLog
		 * @param[in] target the target device for this operator
		 * @param[in] wIn the width of the mantissa input
		 * @param[in] wOut the width of the mantissa output
		 * @param[in] wCount the numbers of bits to count, often equal to wIn but sometimes less (see FPLog)
		 * @param[in] computeSticky Should the operator compute a sticky bit out of the shifted-out bits?
		 * @param[in] countType 0: count zeroes, 1: count ones; -1: have a dynamic OZb input that tells what to count 
		 */
		LZOCShifterSticky(OperatorPtr parentOp, Target* target, int wIn, int wOut, int wCount, bool compute_sticky=false, const int countType=-1);
	
		/** The LZOCShifterSticky destructor */
		~LZOCShifterSticky();

	
		/** Returns the number of bits of the count
		 * @return the number of bits of the count
		 */
		int getCountWidth() const;
	
	
	
	
		void emulate(TestCase* tc);


				/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();

	private:
	
		int          wIn_;                   /**< The number of bits of the input */
		int          wOut_;                  /**< The number of bits of the shifted output */
		int          wCount_;                /**< The number of bits of the count */
		bool         computeSticky_;         /**< If true, compute the sticky bit. If false, save this hardware */
		int          countType_;             /**< -1|0|1. If 0, count zeroes (LZC). If 1, count ones (LOC). If -1, generic LZOC is instatiated, with an input stating what to count */
		string       level_[42];             /**< The names of the signals, just to make code more readable */ 
		string       leveld_[42];            /**< Same but possibly delayed  */
		int          size_[42];              /**< Their size. Do we need to count more than 2^42 bits in FloPoCo? */      
		bool         levelRegistered_ [42]; /**< if boolean true, the corresponding level signal is registered*/ 
		int          countDepth_[42];        /**< the depths for the levels of the architecture */	
		mpz_class    maxValue_;              /**< utilitary var */
	};
}
#endif
