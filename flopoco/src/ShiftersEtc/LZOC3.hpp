#ifndef LZOC3_HPP
#define LZOC3_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "Operator.hpp"
#include "Table.hpp"

namespace flopoco{

	/** The Leading zero counter class.  
	 * Recursive structure with intlog2(wIn) stages.
	 */
	class LZOC3 : public Operator{

	public:
		/** The LZOC3 constructor
		 * @param[in] target the target device for this operator
		 * @param[in] wIn the width of the input
		 * @param[in] useMaxLut wether to use or not the maximal unrouted lut
		 * size
		 */
		LZOC3(Operator* parentOp, Target* target, int wIn, bool useMaxLut);
	
		/** The LZOC3 destructor	*/
		~LZOC3();

		/**
		 * Emulate a correctly rounded division using MPFR.
		 * @param tc a TestCase partially filled with input values 
		 */
		void emulate(TestCase * tc);
	
	protected:
		int wIn_;    /**< The width of the input */
		int wOut_;   /**< The width of the output */
		int blockSize_;
		int lowBitCodeSize_;


	private:
		/**
		 * Generate the encoding table which consists in a string starting with
		 * two indicator bits set to 11 if the block is full of 0, 01 else and
		 * then the number of zeros modulo bockSize encoded as an unsigned
		 * integer
		 *
		 * @param blocksize Table Input Size
		 * @param precomputed number of bits to store the number of zeros
		 */
		Table* buildEncodingTable(int blockSize, int nbLowBits);

		/**
		 * Generate the cleaning tables which return zero for all strings which
		 * contains more than one bit set
		 * @param lutSize the entry size for the table
		 */
		Table* buildDecodingTable(int lutSize);

		/** Take care of generating the XOR for finding the high weight bits of
		 * the output
		 * @param entryName the name of the signal to read from
		 * @param outName the name of the signal to plug the output to
		 * @param totalSize 
		 */
		void generateFinalBits(const string & entryName, const string& outName, int totalSize);

		void simpleTableCounting();

	public:
		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();
	};

}
#endif
