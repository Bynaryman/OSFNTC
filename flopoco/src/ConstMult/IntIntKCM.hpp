#ifndef IntIntKCMS_HPP
#define IntIntKCMS_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <gmpxx.h>
#include "../Operator.hpp"

#include "../BitHeap/BitHeap.hpp"
#include "../Signal.hpp"

namespace flopoco{
/** The IntIntKCM class.
*/
	class IntIntKCM : public Operator
	{
	public:
		/**
		 * @brief The IntIntKCM constructor
		 * @param[in] target the target device
		 * @param[in] wIn    the with of the input
		 * @param[in] C      the constant
		 * @param[in] inputDelays the delays for each input
		 **/
		IntIntKCM(OperatorPtr parentOp, Target* target, int wIn, mpz_class C, bool inputTwosComplement=false);

		IntIntKCM(Operator* parentOp_ Signal* multiplicandX, int wIn, mpz_class C, bool inputTwosComplement=false, BitHeap* bitheap_ = NULL);

		/**
		 * @brief Destructor
		 */
		~IntIntKCM();

		void emulate(TestCase* tc);
		
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target* target, vector<string>& args			);
		
		static TestList unitTest(int index);
		
		static void registerFactory();
		
		
		int getOutputWidth();				/**< returns the number of bits of the output */
		
	protected:
		int wIn_;							/**< the width for the input X*/
		bool signedIn_;
		int chunkSize_;						/**< the size of the lut> **/
		mpz_class C_;						/**< the constant to be used for the multiplication*/
		int wOut_;

		BitHeap*	bitHeap;    			/**< The heap of weighted bits that will be used to do the additions */
		Operator*	parentOp;				/**< The operator which envelops this constant multiplier */

	private:

};

}
#endif
