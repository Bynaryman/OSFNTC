#ifndef OPTIMALCOMPRESSIONSTRATEGY_HPP
#define OPTIMALCOMPRESSIONSTRATEGY_HPP

#include "BitHeap/CompressionStrategy.hpp"
#include "BitHeap/BitHeap.hpp"
#include "BitHeap/Compressor.hpp"

#ifdef HAVE_SCALP
#include <ScaLP/Solver.h>
#include <ScaLP/Exception.h>    // ScaLP::Exception
#include <ScaLP/SolverDynamic.h> // ScaLP::newSolverDynamic
#endif //HAVE_SCALP

namespace flopoco
{

class BitHeap;

	class OptimalCompressionStrategy : public CompressionStrategy
	{
	public:



		/**
		 * A basic constructor for a compression strategy
		 */
		OptimalCompressionStrategy(BitHeap *bitheap, bool optimalMinStages=false);



	private:
		bool optimalMinStages;

		/**
		 *	@brief starts the compression algorithm. It will call parandehAfshar()
		 */
		void compressionAlgorithm();

#ifdef HAVE_SCALP

		bool optimalGeneration(unsigned int stages = 0, bool optimalMinStages = false);

		void resizeBitAmount(unsigned int stages);

		void initializeSolver();

		/**
		 *	@brief initializes all needed variables (k, N, U, D)
		 */
		void initializeVariables();

		/**
		 *	@brief generates objective. The constraint minimizes the sum of the area of all used compressors
		 */
		void generateObjective();

		/**
		 *	@brief sets up all the U's. Those are the inputs in every stage and column which the bitheawp receives.
		 */
		void generateConstraintC0();

		/**
		 *	@brief makes sure, that all bits at each position are covered with compressorsinputs
		 */
		void generateConstraintC1();

		/**
		 *	@brief sets every N_s_c to the amount of outputbits at its position
		 */
		void generateConstraintC2();

		/**
		 *	@brief makes sure that if stage s is the stage where the final adder is placed, all of the columns do not violate the necessary constraints
		 */
		void generateConstraintC3();

		/**
		 *	@brief allows only one stage to be the stage with the final adder
		 */
		void generateConstraintC4();

		/**
		 *	@brief sets the given stage as stage with the final decoder
		 *	@param stage is the selected stage for the final adder
		 */
		void selectOutputStage(unsigned int stage);

		/**
		 *	@brief adds flipflop at back if there is not present in possible compressors. If flipflop is added, returns true, otherwise false. stores the found or created flipflop.
		 */
		bool addFlipFlop();

		void generateConstraintForVariableCompressors();

		bool solve();

		void fillSolutionFromILP();

		unsigned int getMaxStageCount();

		unsigned int getMinAmountOfStages();

		bool daddaTwoBitStageReached(vector<int> currentBits, unsigned int stage);

		ScaLP::Solver *problemSolver; /* stores the solver */

		vector<vector<vector<ScaLP::Variable> > > compCountVars; /* stores k_s_e_c: the integer variable, which counts how many compressors fro mtype e are in stage s and column c. s is the first index, e the second and c the third. */

		vector<vector<ScaLP::Variable> > columnBitCountVars; /* stores N_s_c: the integer variable, which counts, how many outputsbits from the compressors from the previous stage are in stage s and column c. s is the first index, c the second */

		vector<ScaLP::Variable> stageVars; /* stores D_s: Binary variable. if D_s is true, stage s is the output stage. */

		vector<vector<ScaLP::Variable> > newBitsCountVars; /* stores U_s_c: integer variable which counts how many bits are added in stage s and column c. Those bits are not from the compressors from the previous stage but from the inputs of the bitheap. first dimension is s, second is c */

		vector<vector<ScaLP::Variable> > emptyInputVars; /* stores Z_s_c: the integer variable, which counts, how many empty inputs are in stage s and column c. First dimension is s, second is c */

		BasicCompressor* flipflop;

#endif //HAVE_SCALP

	};

}
#endif
