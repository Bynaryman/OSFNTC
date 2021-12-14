
#include "MaxEfficiencyCompressionStrategy.hpp"
//#include "CompressionStrategy.hpp"
//#include "BitHeap/BitHeap.hpp"


using namespace std;

namespace flopoco{


	MaxEfficiencyCompressionStrategy::MaxEfficiencyCompressionStrategy(BitHeap* bitheap) : CompressionStrategy(bitheap)
	{
		lowerBounds.resize(1);
		lowerBounds[0] = 0.0;
	}




	void MaxEfficiencyCompressionStrategy::compressionAlgorithm()
	{
		REPORT(DEBUG, "compressionAlgorithm is maxEfficiency");

		//for the maxEfficiency algorithm, the compressors should be ordered by efficiency
		orderCompressorsByCompressionEfficiency();

		//adds the Bits to stages and columns
		orderBitsByColumnAndStage();

		//populates bitAmount. on this simple structure the maxEfficiency algorithm is working
		fillBitAmounts();

		//prints out how the inputbits of the bitheap looks like
		printBitAmounts();

		//new solution
		solution = BitHeapSolution();
		solution.setSolutionStatus(BitheapSolutionStatus::HEURISTIC_PARTIAL);

		//generates the compressor tree. Works only on bitAmount, compressors will be put into solution
		maxEfficiencyAlgorithm();

		//reports the area in LUT-equivalents
        printSolutionStatistics();

		//here the VHDL-Code for the compressors as well as the bits->compressors->bits are being written.
		applyAllCompressorsFromSolution();

	}

	void MaxEfficiencyCompressionStrategy::maxEfficiencyAlgorithm(){


		unsigned int s = 0;
		while(true){

			//before we start this stage, check if compression is done
			if(checkAlgorithmReachedAdder(2, s)){
				break;
			}

			//make sure there is the stage s+1 with the same amount of columns as s
			while(bitAmount.size() <= s + 1){
				bitAmount.resize(bitAmount.size() + 1);
				bitAmount[bitAmount.size() - 1].resize(bitAmount[bitAmount.size() - 2].size(), 0);
			}

			bool found = true;
			while(found){
				found = false;

				double achievedEfficiencyBest = -1.0;
				BasicCompressor* compressor = nullptr;
				unsigned int column = 0;

				for(unsigned int e = 0; e < possibleCompressors.size(); e++){
					BasicCompressor* currentCompressor = possibleCompressors[e];
					REPORT(DEBUG, "compressor is " << currentCompressor->getStringOfIO());
					vector<bool> used;
					used.resize(bitAmount[s].size(), false);

					unsigned int columnsAlreadyChecked = 0;
					//check if the achievedEfficiency is better than the maximal efficiency possible by this compressor. If true, it's not necessary to check this and the following compressors. Therefore return.
					while(columnsAlreadyChecked < bitAmount[s].size() && !((found == true) && currentCompressor->getEfficiency() - achievedEfficiencyBest < 0.0001)){

						unsigned int currentMaxColumn = 0;
						int currentSize = 0;
						for(unsigned int c = 0; c < bitAmount[s].size(); c++){
							if(!used[c] && bitAmount[s][c] > currentSize){
								currentMaxColumn = c;
								currentSize = bitAmount[s][c];
							}
						}
						used[currentMaxColumn] = true;
						double achievedEfficiencyCurrent = getCompressionEfficiency(s, currentMaxColumn, currentCompressor);
						REPORT(FULL, "checked " << currentCompressor->getStringOfIO() << " in stage " << s << " and column " << currentMaxColumn << " with an efficiency of " << achievedEfficiencyCurrent);

						float lowerBound;
						if(s < lowerBounds.size())
							lowerBound = lowerBounds[s];
						else
							lowerBound = 0.0;

						if(achievedEfficiencyCurrent > (achievedEfficiencyBest + 0.0001) && achievedEfficiencyCurrent > (lowerBound - 0.0001)){
							achievedEfficiencyBest = achievedEfficiencyCurrent;
							compressor = currentCompressor;
							found = true;
							column = currentMaxColumn;
						}
						columnsAlreadyChecked++;
					}
				}
				if(found){
					REPORT(DETAILED, "placed compressor " << compressor->getStringOfIO() << " in stage " << s << " and column " << column);
					REPORT(DETAILED, "efficiency is " << achievedEfficiencyBest);
					placeCompressor(s, column, compressor);
				}
			}
			//finished one stage. bring the remaining bits in bitAmount to the new stage
			for(unsigned int c = 0; c < bitAmount[s].size(); c++){
				if(bitAmount[s][c] > 0){
					bitAmount[s + 1][c] += bitAmount[s][c];
					bitAmount[s][c] = 0;
				}
				solution.setEmptyInputsByRemainingBits(s, bitAmount[s]);
			}
			REPORT(DEBUG, "finished stage " << s);
			printBitAmounts();
			s++;
		}

	}








}
