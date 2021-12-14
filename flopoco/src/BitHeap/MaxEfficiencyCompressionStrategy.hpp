#ifndef MAXEFFICIENCYCOMPRESSIONSTRATEGY_HPP
#define MAXEFFICIENCYCOMPRESSIONSTRATEGY_HPP

#include "BitHeap/CompressionStrategy.hpp"
#include "BitHeap/BitHeap.hpp"

namespace flopoco
{

class BitHeap;

	class MaxEfficiencyCompressionStrategy : public CompressionStrategy
	{
	public:

		/**
		 * A basic constructor for a compression strategy
		 */
		MaxEfficiencyCompressionStrategy(BitHeap *bitheap);


	private:
		/**
		 *	@brief starts the compression algorithm. It will call maxEfficiencyAlgorithm()
		 */
		void compressionAlgorithm();

		/**
		 * generates the compressor tree
		 */
		void maxEfficiencyAlgorithm();

		vector<float> lowerBounds;


	};

}
#endif
