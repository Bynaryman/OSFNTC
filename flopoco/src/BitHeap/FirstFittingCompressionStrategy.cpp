
#include "FirstFittingCompressionStrategy.hpp"


using namespace std;

namespace flopoco{


	FirstFittingCompressionStrategy::FirstFittingCompressionStrategy(BitHeap* bitheap) : CompressionStrategy(bitheap)
	{

	}




	void FirstFittingCompressionStrategy::compressionAlgorithm()
	{
		REPORT(DEBUG, "compressionAlgorithm is FirstFitting");
		double delay = compressionDelay;
		unsigned int colorCount = 0;

		while(bitheap->compressionRequired())
		{
			Bit *soonestBit, *soonestCompressibleBit;

			//get the soonest bit in the bitheap
			soonestBit = getSoonestBit(0, bitheap->width-1);
			//get the soonest compressible bit in the bitheap
			soonestCompressibleBit = getSoonestCompressibleBit(0, bitheap->width-1, delay);
			REPORT(DEBUG, "before compress");
			//apply as many compressors as possible, with the current delay
			bool bitheapCompressed = compress(delay, soonestCompressibleBit);
			REPORT(DEBUG, "after compress");
			//take a snapshot of the bitheap, if a compression was performed
			//	including the bits that are to be removed
			if(bitheapCompressed == true)
			{
				//color the newly added bits
				colorCount++;
				bitheap->colorBits(BitType::justAdded, colorCount);
				//take a snapshot of the bitheap
				bitheapPlotter->takeSnapshot(soonestBit, soonestCompressibleBit);
			}

			//remove the bits that have just been compressed
			bitheap->removeCompressedBits();

			//mark the bits that have just been added as free to be compressed
			bitheap->markBitsForCompression();

			//take a snapshot of the bitheap, if a compression was performed
			//	without the bits that have been removed
			//	with the newly added bits as available for compression
			if(bitheapCompressed == true)
			{
				//take a snapshot of the bitheap
				bitheapPlotter->takeSnapshot(soonestBit, soonestCompressibleBit);
			}
			//send the parts of the bitheap that has already been compressed to the final result
			concatenateLSBColumns();

			//print the status of the bitheap
			bitheap->printBitHeapStatus();

			//if no compression was performed, then the delay between
			//	the soonest bit and the rest of the bits that need to be compressed needs to be increased
			if(bitheapCompressed == false)
			{
				//passing from the delay of a compressor to a delay equal to a period, with the target FPGA
				//	then increase the delay progressively with a delay equal to a period
				if(delay == compressionDelay)
					delay = 1.0 / bitheap->getOp()->getTarget()->frequency();
				else
					delay += 1.0 / bitheap->getOp()->getTarget()->frequency();
			}
		}

        //reports the area in LUT-equivalents
        printSolutionStatistics();
	}

}
