#ifndef FIRSTFITTINGCOMPRESSIONSTRATEGY_HPP
#define FIRSTFITTINGCOMPRESSIONSTRATEGY_HPP

#include "BitHeap/CompressionStrategy.hpp"
#include "BitHeap/BitHeap.hpp"

namespace flopoco
{

class BitHeap;

	class FirstFittingCompressionStrategy : public CompressionStrategy
	{
	public:

		/**
		 * A basic constructor for a compression strategy
		 */
		FirstFittingCompressionStrategy(BitHeap *bitheap);

	private:
		void compressionAlgorithm();
	};

}
#endif
