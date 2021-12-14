#ifndef IntKaratsuba_HPP
#define IntKaratsuba_HPP

#include "Operator.hpp"
#include "BitHeap/BitHeap.hpp"

namespace flopoco{

    class IntKaratsubaRectangular : public Operator
	{
	public:
		/** 
         * The constructor of the IntKaratsubaRectangular class
		 **/
		IntKaratsubaRectangular(Operator* parentOp, Target* target, int wX, int wY,bool useKaratsuba=true, bool useRectangularTiles=true);

        /**
         * Emulates the multiplier
		 */
		void emulate(TestCase* tc);

        /** Factory method */
        static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
        /** Register the factory */
        static void registerFactory();

		static TestList unitTest(int index);

    protected:
	
        int wX; /**< the width (in bits) of the input X  */
        int wY; /**< the width (in bits) of the input X  */
        int wOut; /**< the width (in bits) of the output R  */

		bool useKaratsuba; /**< uses Karatsuba when true, instead uses standard tiling without sharing */
		bool useRectangularTiles;
		double multDelay;

		int TileBaseMultiple;
		int TileWidthMultiple;
		int TileHeightMultiple;
		int TileWidth;
		int TileHeight;

		BitHeap *bitHeap;

		/**
		 * Implements "+ a_i b_j" using a rectangular multiplier
		 */
		void createMult(int i, int j);

		/**
		 * Implements a_i b_j + a_k b_l by using (a_i - a_k)(b_j - b_l) + a_i b_l + a_k b_j
		 * using a rectangular multiplier
		 */
		void createRectKaratsuba(int i, int j, int k, int l);

		/**
		 * Implements a_i b_j + a_k b_l by using (a_i - a_k)(b_j - b_l) + a_i b_l + a_k b_j
		 * using a square multiplier
		 */
		void createSquareKaratsuba(int i, int j, int k, int l);

	private:
	};

}
#endif
