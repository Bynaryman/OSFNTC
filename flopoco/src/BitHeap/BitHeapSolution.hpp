#ifndef SOLUTION_HPP
#define SOLUTION_HPP

#include "BitHeap/Compressor.hpp"
#include <string>
namespace flopoco
{

	enum BitheapSolutionStatus {
		EMPTY,
		NO_COMPRESSION_NEEDED,
		HEURISTIC_PARTIAL,
		HEURISTIC_COMPLETE,
		OPTIMAL_PARTIAL,
		OPTIMAL_COMPLETE,
		MIXED_PARTIAL,
		MIXED_COMPLETE
	};

	class BitHeapSolution
	{
	public:

		/**
		 * A basic constructor for a solution
		 */
		BitHeapSolution();

		/**
		 *	@brief adds a compressor at a given stage and column
		 *	@param stage specifies the stage where the compressor is added
		 *	@param column specifies the column where the compressor is added
		 *	@param compressor pointer to the BasicCompressor
		 *	@param middleLength If BasicCompressor is a variable Compressor, its middleLength will
		 *  	be specified with this argument. Otherwise middleLength is zero.
		 */
		void addCompressor(unsigned int stage, unsigned int column, BasicCompressor* compressor, int middleLength = 0);

		/**
		 *	@brief resturns a vector of all compressors and their middleLength in a given position.
		 *		The position is specified by stage and column
		 *	@param stage specifies the stage
		 *	@param column specifies the column
		 */
		vector<pair<BasicCompressor*, unsigned int> > getCompressorsAtPosition(unsigned int stage, unsigned int column);

		/**
		 *	@brief returns number of stages
		 */
		int getNumberOfStages();

		/**
		 *	@brief returns number of columns for a given stage
		 *	@param stage specifies the stage
		 */
		int getNumberOfColumnsAtStage(unsigned int stage);

		/**
		 *	@brief sets the solutionStatus
		 *	@param stat the new status
		 */
		void setSolutionStatus(BitheapSolutionStatus stat);

		/**
		 *	@brief sets the solution as complete
		 */
		void markSolutionAsComplete();

		/**
		 *	@brief gets the solutionStatus
		 */
		BitheapSolutionStatus getSolutionStatus();

		/**
		 *	@brief sets the emptyInputs of a certain stage by the remainingBits
		 *	@param stage the stage were the emptyInputs should been set
		 *	@param remainingBits is the vector of remainingBits.
		 */
		void setEmptyInputsByRemainingBits(unsigned int stage, vector<int> remainingBits);

		/**
		 *	@brief returns for the given stage the amount of empty inputs for every column
		 *	@param stage the stage for which the empty inputs should be given
		 */
		vector<unsigned int> getEmptyInputsByStage(unsigned int stage);

	private:

		/* Entry is the struct which specifies compressors in a solution*/
		struct Entry {
			unsigned int column;
			unsigned int stage;
			unsigned int amount;
			BasicCompressor* compressor;
			int middleLength;
		};

		/* three-dimensional vector of compressors in the solution.
			first dimension is the stage,
			second the column and
			third is a vector of all compressors*/
		vector<vector<vector<Entry> > > comps;

		/** specifies, how many inputs of the compressors at this stage and columns are equals zero */
		vector<vector<unsigned int> > emptyInputs;

		BitheapSolutionStatus status;



	};

}
#endif
