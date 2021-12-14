
#include "BitHeapSolution.hpp"


using namespace std;

namespace flopoco{


	BitHeapSolution::BitHeapSolution()
	{
		status = BitheapSolutionStatus::EMPTY;
	}

	void BitHeapSolution::addCompressor(unsigned int stage, unsigned int column, BasicCompressor* compressor, int middleLength){

		//make sure that the position (stage and column) exist
		if(stage >= comps.size()){
			comps.resize(stage + 1);
		}
		if(column >= comps[stage].size()){
			comps[stage].resize(column + 1);
		}

		for(unsigned int i = 0; i < comps[stage][column].size(); i++){
			if(comps[stage][column][i].compressor == compressor){
				if(comps[stage][column][i].middleLength == middleLength){
					comps[stage][column][i].amount += 1;
					return;
				}
			}
		}

		//new Entry, because it does not exists
		Entry tempEntry;
		tempEntry.stage = stage;
		tempEntry.column = column;
		tempEntry.compressor = compressor;
		tempEntry.amount = 1;
		tempEntry.middleLength = middleLength;
		comps[stage][column].push_back(tempEntry);

	}




	vector<pair<BasicCompressor*, unsigned int> > BitHeapSolution::getCompressorsAtPosition(unsigned int stage, unsigned int column){

		if(comps.size() > stage && comps[stage].size() > column){
			vector<pair<BasicCompressor*, unsigned int> > returnList;

			for(unsigned int i = 0; i < comps[stage][column].size(); i++){
				for(unsigned int j = 0; j < comps[stage][column][i].amount; j++){
					pair<BasicCompressor*, unsigned int> tempPair;
					tempPair.first =  comps[stage][column][i].compressor;
					tempPair.second = comps[stage][column][i].middleLength;
					returnList.push_back(tempPair);
				}
			}
			return returnList;
		}
		else{
			vector<pair<BasicCompressor*, unsigned int> > emptyList;
			return emptyList;
		}
	}

	int BitHeapSolution::getNumberOfStages(){
		return comps.size();
	}
	int BitHeapSolution::getNumberOfColumnsAtStage(unsigned int stage){
		if(comps.size() >= stage){
			return -1;
		}
		return comps[stage].size();
	}

	vector<unsigned int> BitHeapSolution::getEmptyInputsByStage(unsigned int stage){
		if(stage > emptyInputs.size()){
			vector<unsigned int> emptyVector;
			return emptyVector;
		}
		else{
			return emptyInputs[stage];
		}
	}

	void BitHeapSolution::setEmptyInputsByRemainingBits(unsigned int stage, vector<int> remainingBits){
		if(emptyInputs.size() <= stage){
			emptyInputs.resize(stage + 1);
		}
		vector<unsigned int> tempVector; //contains the new values
		for(unsigned int c = 0; c < remainingBits.size(); c++){
			if(remainingBits[c] >= 0){
				tempVector.push_back(0);
			}
			else if(remainingBits[c] < 0){
				int amount = remainingBits[c] * (-1);
				tempVector.push_back((unsigned int) amount);
			}
		}
		emptyInputs[stage] = tempVector;
	}

	BitheapSolutionStatus BitHeapSolution::getSolutionStatus(){
		return status;
	}

	void BitHeapSolution::setSolutionStatus(BitheapSolutionStatus stat){
		status = stat;
	}

	void BitHeapSolution::markSolutionAsComplete(){
		if(status == BitheapSolutionStatus::MIXED_PARTIAL){
			status = BitheapSolutionStatus::MIXED_COMPLETE;
		}
		else if(status == BitheapSolutionStatus::OPTIMAL_PARTIAL){
			status = BitheapSolutionStatus::OPTIMAL_COMPLETE;
		}
		else if(status == BitheapSolutionStatus::HEURISTIC_PARTIAL){
			status = BitheapSolutionStatus::HEURISTIC_COMPLETE;
		}
		else if(status == BitheapSolutionStatus::EMPTY){
			cout << "tried to mark a Solution as complete, although it is empty" << endl;
		}
		//else: already complete/compression not needed.
	}





}
