#ifndef COMPRESSIONSTRATEGY_HPP
#define COMPRESSIONSTRATEGY_HPP


#include <vector>
#include <sstream>

#include "Operator.hpp"

#include "BitHeap/Bit.hpp"
#include "BitHeap/Compressor.hpp"
#include "BitHeap/BitHeap.hpp"
#include "BitHeap/BitHeapPlotter.hpp"
#include "BitHeap/BitHeapSolution.hpp"

#include "IntAddSubCmp/IntAdder.hpp"

namespace flopoco
{

class Bit;
class Compressor;
class IntAdder;
class BitheapPlotter;

	class CompressionStrategy
	{
	public:

		/**
		 * A basic constructor for a compression strategy
		 */
		CompressionStrategy(BitHeap *bitheap);

		/**
		 * Destructor
		 */
		~CompressionStrategy();


		/**
		 * Start compressing the bitheap
		 */
		void startCompression();


	protected:

		/**
		 * @brief Uses an algorithm, which decides how the compression is being done
		 */
		virtual void compressionAlgorithm() = 0;

		/**
		 * @brief start a new round of compression of the bitheap using compressors
		 *        only bits that are within at most a given delay from the soonest
		 *        bit are compressed
		 * @param delay the maximum delay between the compressed bits and the soonest bit
		 * @return whether a compression has been performed
		 * @param soonestCompressibleBit the earliest bit in the bitheap that is compressible
		 */
		bool compress(double delay, Bit *soonestCompressibleBit);

		/**
		 * @brief apply a compressor to the column given as parameter
		 * @param bitVector the bits to compress
		 * @param compressor the compressor used for compression
		 * @param weight the weight of the lsb column of bits
		 */
		void applyCompressor(vector<Bit*> bitVector, Compressor* compressor, int weight);

		/**
		 * @brief apply a compressor to the column given as parameter. Can handle
		 * 		  multiple outputbits per column as well as unused inputs.
		 * @param bitVector the bits to compress. First dimension is the column
		 * @param compressor the compressor used for compression
		 * @param weight the weight of the lsb column of bits
		 */
		void applyCompressor(vector<vector<Bit*> > bitVector, Compressor* compressor, int weight);

		/**
		 * @brief applies an adder with wIn = msbColumn-lsbColumn+1;
		 * lsbColumn has size=3, if hasCin=true, and the other columns (including msbColumn) have size=2
		 * @param msbColumn the msb of the addends
		 * @param lsbColumn the lsb of the addends
		 * @param hasCin whether the lsb column has size 3, or 2
		 */
		void applyAdder(int msbColumn, int lsbColumn, bool hasCin = true);

		/**
		 * @brief compress the remaining columns using adders
		 */
		void applyAdderTreeCompression();

		/**
		 * @brief generate the final adder for the bit heap
		 * (when the columns height is maximum 2/3)
		 * @param isXilinx whether the target FPGA is a Xilinx device
		 *        (different primitives used)
		 */
		void generateFinalAddVHDL(bool isXilinx = true);

		/**
		 * @brief computes the latest bit from the bitheap, between columns lsbColumn and msbColumn
		 * @param msbColumn the weight of the msb column
		 * @param lsbColumn the weight of the lsb column
		 */
		Bit* getLatestBit(unsigned lsbColumn, unsigned msbColumn);

		/**
		 * @brief computes the soonest bit from the bitheap, between columns lsbColumn and msbColumn
		 *        if the bitheap is empty (or no bits are available for compression), nullptr is returned
		 * @param msbColumn the weight of the msb column
		 * @param lsbColumn the weight of the lsb column
		 */
		Bit* getSoonestBit(unsigned lsbColumn, unsigned msbColumn);

		/**
		 * @brief computes the soonest compressible bit from the bitheap, between columns lsbColumn and msbColumn
		 *        if the bitheap is empty (or no bits are available for compression), nullptr is returned
		 * @param msbColumn the weight of the msb column
		 * @param lsbColumn the weight of the lsb column
		 * @param delay the maximum delay between the compressed bits
		 */
		Bit* getSoonestCompressibleBit(unsigned lsbColumn, unsigned msbColumn, double delay);

		/**
		 * @brief can the compressor given as parameter be applied to the column
		 *        given as parameter, compressing bits within the given delay from
		 *        the soonest bit, given as parameter
		 * @param columnNumber the column being compressed (column index)
		 * @param compressor the compressor being used for compression (compressor index)
		 * @param soonestBit the bit used as reference for timing
		 * @param delay the maximum delay between the bits compressed and the reference bit
		 * @return a vector containing the bits that can be compressed, or an empty vector
		 *         if the compressor cannot be applied
		 */
		vector<Bit*> canApplyCompressor(unsigned columnNumber, unsigned compressorNumber, Bit* soonestBit, double delay);

		/**
		 * @brief generate all the compressors that will be used for
		 * compressing the bitheap
		 */
		void generatePossibleCompressors();

		/**
		 * @brief return the delay of a compression stage
		 * (there can be several compressions staged in a cycle)
		 */
		double getCompressorDelay();

		/**
		 * @brief verify up until which lsb column the compression
		 * has already been done and concatenate those bits
		 */
		void concatenateLSBColumns();

		/**
		 * @brief concatenate all the chunks of the compressed bitheap and create the final result
		 */
		void concatenateChunks();


		/**
		 * @brief fills orderedBits dependent on the frequency and the compression delay (and therefore the stagesPerCycle)
		*/
		void orderBitsByColumnAndStage();

		/**
		 * @brief orders possibleCompressors by compressionRatio.
		 */

		void orderCompressorsByCompressionEfficiency();

		/**
		 * @brief fills bitAmount with the number of bits at that position (stage and column).
		 */
		void fillBitAmounts();

		/**
		 * @brief returns compression efficiency, if the compressor is placed at this stage and column.
		 * @param stage the stage where the compressor shoould be placed
		 * @param column the column where the compressor should be placed
		 * @param compressor the pointer to the basicCompressor
		 * @param middleLength if the compressor is variable, the middlelength specifies how many of the
		 * 		mid-parts are there. If compressor is of type combinatorial or vendor-specific, middlepart
		 * 		must be 0.
		 */

		double getCompressionEfficiency(unsigned int stage, unsigned int column, BasicCompressor* compressor, unsigned int middleLength = 0);

		/**
		 * @brief places the compressor. Does not generate VHDL. It just sets the corresponding bitAmounts
		 * 		right and adds this compressor to the solution.
		 * @param stage the stage where the compressor is placed
		 * @param column the column where the compressor is placed
		 * @param compressor the pointer to the basicCompressor
		 * @param middleLength if the compressor is variable, the middlelength specifies how many of the
		 * 		mid-parts are there. If compressor is of type combinatorial or vendor-specific, middlepart
		 * 		must be 0.
		 */
		void placeCompressor(unsigned int stage, unsigned int column, BasicCompressor* compressor, unsigned int middleLength = 0);

		/**
		 *	@brief calls setHasBeenScheduled() for all the bits in the vector
		 *	@param bitVector the vector of bits which should be set to scheduled
		 */
		void setScheduleOfBits(vector<vector<Bit*> > bitVector);

		/**
		 *	@brief Returns the stage of arrival for a given bit.
		 */
		unsigned int getStageOfArrivalForBit(Bit* bit);

		/**
		 * @brief prints the current bitAmounts for all stages and columns (for debugging)
		 */
		void printBitAmounts();

		/**
		 *	@brief prints the current Bits in the bitheap (for debugging)
		 */
		void printBitsInBitheap();

        /**
          * @brief prints the Area of Compression
          */
        void printSolutionStatistics();



        /**
		 * @brief all compressors specified in the solution will be used (vhdl code will be generated)
		 */
		void applyAllCompressorsFromSolution();


		/**
		 *	@brief checks if the algorithm which places the compressors is finished (a stage where
		 		the	final adder can be used has been reached). Works on the data of bitAmount
			@param adderHeight what is the adderHeight of the final adder (e.g. 2 or 3)
			@param stage specifies in which stage the check for the final adder should be done. To pass
				this test there are no bits in other stages allowed
		 */
		bool checkAlgorithmReachedAdder(unsigned int adderHeight, unsigned int stage);

		vector<vector<vector<Bit*> > > orderedBits; /**< The bits of the bitheap ordered by stages. First dimension is the stage, second the column */

		vector<vector<int> > bitAmount; 			/**< Amount of bits in each stage and column. The compression strategies (currently FirstFitting does not) work on this bitAmount, and if a solution is finished, the compressors will be used. */

		BitHeapSolution solution;

		BitHeap *bitheap;                        /**< The bitheap this compression strategy belongs to */
		BitheapPlotter *bitheapPlotter;             /**< The bitheap plotter for this bitheap compression */

		vector<BasicCompressor*> possibleCompressors;    /**< All the possible compressors that can be used for compressing the bitheap */

		unsigned compressionDoneIndex;              /**< The index in the range msb-lsb up to which the compression is completed */
		vector<string> chunksDone;                  /**< The vector containing the chunks of the compression result */

		int stagesPerCycle;                         /**< The number of stages of compression in each cycle */
		double compressionDelay;                    /**< The duration of a compression stage */

		// For error reporting to work
		int guid;
		string srcFileName;
		string uniqueName_;



	};
}

#endif
