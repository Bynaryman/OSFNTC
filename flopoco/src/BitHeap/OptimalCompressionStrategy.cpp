
#include "OptimalCompressionStrategy.hpp"



using namespace std;

namespace flopoco{


	OptimalCompressionStrategy::OptimalCompressionStrategy(BitHeap* bitheap, bool optimalMinStages) : CompressionStrategy(bitheap)
	{
		this->optimalMinStages = optimalMinStages;
	}




	void OptimalCompressionStrategy::compressionAlgorithm()
	{
		REPORT(DEBUG, "compressionAlgorithm is optimal");

#ifndef HAVE_SCALP
		THROWERROR("For the optimal compressor tree generation scalp is needed");
#else

		//for debugging it might be better to order the compressors by efficiency
		orderCompressorsByCompressionEfficiency();

		//adds the Bits to stages and columns
		orderBitsByColumnAndStage();

		//populates bitAmount
		fillBitAmounts();

		//prints out how the inputBits of the bitheap looks like
		printBitAmounts();

		//new solution
		solution = BitHeapSolution();
		solution.setSolutionStatus(BitheapSolutionStatus::OPTIMAL_PARTIAL);

		//generates the compressor tree but only works one the bitAmount datastructure. Fills the solution. No VHDL-Code is written here.

		bool foundSolution = false;
		if(!optimalMinStages){
			foundSolution = optimalGeneration();
			if(foundSolution == false){
				THROWERROR("wasn't able to find a solution within the given timelimit");
			}
		}
		else{
			unsigned int stages = getMinAmountOfStages();
			REPORT(DEBUG, "after getMinAmountOfStages stages = " << stages);
			bool foundSolution = false;
			while(!foundSolution){
				foundSolution = optimalGeneration(stages, true);
				stages++;
			}

		}

        //reports the area in LUT-equivalents
        printSolutionStatistics();

		//here the VHDL-Code for the compressors as well as the bits->compressors->bits are being written.
		applyAllCompressorsFromSolution();
#endif //HAVE_SCALP
	}

#ifdef HAVE_SCALP
	bool OptimalCompressionStrategy::optimalGeneration(unsigned int stages, bool optimalMinStages){

		if(!optimalMinStages){
			unsigned int daddaStageCount = getMaxStageCount();

			REPORT(DEBUG, "daddaStageCount is " << daddaStageCount);

			resizeBitAmount(daddaStageCount);//set bitAmounts to 5 stages
		}
		else{
			resizeBitAmount(stages);
		}

		REPORT(DEBUG, "bitAmount has now a size of " << bitAmount.size());
		REPORT(DEBUG, "resized bitAmount");

		initializeSolver();
		REPORT(DEBUG, "initialized solver");

		addFlipFlop();
		REPORT(DEBUG, "added flipflop");

		initializeVariables();
		REPORT(DEBUG, "initialized variables");

		generateObjective();
		REPORT(DEBUG, "generated objective");

		generateConstraintC0();
		REPORT(DEBUG, "finished constraint C0");

		generateConstraintC1();
		REPORT(DEBUG, "finished constraint C1");

		generateConstraintC2();
		REPORT(DEBUG, "finished constraint C2");

		generateConstraintC3();
		REPORT(DEBUG, "finished constraint C3");

		generateConstraintC4();
		REPORT(DEBUG, "generated all constraints");

		if(optimalMinStages){
			selectOutputStage(stages);
		}

		//selectOutputStage(); needed for optimalMinStages()

		generateConstraintForVariableCompressors();

		problemSolver->writeLP("compressorTree.lp");

		bool success = solve();
		REPORT(DEBUG, "solved with success = " << success);
		if(success){
			fillSolutionFromILP();
			REPORT(DEBUG, "solution done from ilp");
		}

		return success;

	}

	void OptimalCompressionStrategy::resizeBitAmount(unsigned int stages){

		stages++;	//we need also one stage for the outputbits

		unsigned int columns = bitAmount[bitAmount.size() - 1].size();
		//we need one stage more for the
		while(bitAmount.size() < stages){
			bitAmount.resize(bitAmount.size() + 1);
			bitAmount[bitAmount.size() - 1].resize(columns, 0);
		}
	}

	void OptimalCompressionStrategy::initializeSolver(){

		problemSolver = new ScaLP::Solver(ScaLP::newSolverDynamic({bitheap->getOp()->getTarget()->getILPSolver(),"Gurobi","CPLEX","SCIP","LPSolve"}));
		problemSolver->timeout = bitheap->getOp()->getTarget()->getILPTimeout();
        REPORT(DEBUG, "timeout is set to " << problemSolver->timeout << " seconds");
	}

	void OptimalCompressionStrategy::initializeVariables(){

		//k_s_e_c
		compCountVars.clear();
		compCountVars.resize(bitAmount.size() - 1);
		for(unsigned int s = 0; s < compCountVars.size(); s++){
			compCountVars[s].resize(possibleCompressors.size());
			for(unsigned int e = 0; e < possibleCompressors.size(); e++){
				for(unsigned int c = 0; c < bitAmount[s].size(); c++){
					stringstream varName;
					varName << "k_" << s << "_" << e << "_" << c;
					ScaLP::Variable tempK = ScaLP::newIntegerVariable(varName.str(), 0, ScaLP::INF());
					compCountVars[s][e].push_back(tempK);

					//TODO: variable compressors
				}
			}
		}
		REPORT(DEBUG, "finished initializing k-variables");

		//N_s_c
		columnBitCountVars.clear();
		columnBitCountVars.resize(bitAmount.size() - 1); //inputs of first stage are in U_0_x
		for(unsigned int s = 1; s < columnBitCountVars.size() + 1; s++){
			for(unsigned int c = 0; c < bitAmount[s].size(); c++){
				stringstream varName;
				varName << "N_" << s << "_" << c;
				ScaLP::Variable tempN = ScaLP::newIntegerVariable(varName.str(), 0, ScaLP::INF());
				columnBitCountVars[s - 1].push_back(tempN);
			}
		}
		REPORT(DEBUG, "finished initializing N-variables");

		//U_s_c
		newBitsCountVars.clear();
		newBitsCountVars.resize(bitAmount.size());
		for(unsigned int s = 0; s < bitAmount.size(); s++){
			for(unsigned int c = 0; c < bitAmount[s].size(); c++){
				stringstream varName;
				varName << "U_" << s << "_" << c;
				ScaLP::Variable tempU = ScaLP::newIntegerVariable(varName.str(), -ScaLP::INF(), ScaLP::INF());
				newBitsCountVars[s].push_back(tempU);
			}
		}
		REPORT(DEBUG, "finished initializing U-variables");

		//Z_s_c
		emptyInputVars.clear();
		emptyInputVars.resize(bitAmount.size());
		for(unsigned int s = 0; s < emptyInputVars.size(); s++){
			for(unsigned int c = 0; c < bitAmount[s].size(); c++){
				stringstream varName;
				varName << "Z_" << s << "_" << c;
				ScaLP::Variable tempZ = ScaLP::newIntegerVariable(varName.str(), 0, ScaLP::INF());
				emptyInputVars[s].push_back(tempZ);
			}
		}
		REPORT(DEBUG, "finished initializing Z-variables");

		//D_s
		stageVars.clear();
		stageVars.resize(bitAmount.size());
		for(unsigned int s = 0; s < stageVars.size(); s++){
			stringstream varName;
			varName << "D_" << s;
			ScaLP::Variable tempD = ScaLP::newBinaryVariable(varName.str());
			stageVars[s] = tempD;
		}
		REPORT(DEBUG, "finished initializing D-variables");

	}

	void OptimalCompressionStrategy::generateObjective(){
		ScaLP::Term objectiveTerm;
		for(unsigned int s = 0; s < compCountVars.size(); s++){
			for(unsigned int e = 0; e < compCountVars[s].size(); e++){
				for(unsigned int c = 0; c < compCountVars[s][e].size(); c++){
					objectiveTerm = objectiveTerm + possibleCompressors[e]->area * compCountVars[s][e][c];
				}
			}
		}

		ScaLP::Objective obj = ScaLP::minimize(objectiveTerm);
		problemSolver->setObjective(obj);
	}

	void OptimalCompressionStrategy::generateConstraintC0(){
		for(unsigned int s = 0; s < bitAmount.size(); s++){
			for(unsigned int c = 0; c < bitAmount[s].size(); c++){
				stringstream consName;
				consName << "C0_" << s << "_" << c;
				ScaLP::Constraint tempConstraint = newBitsCountVars[s][c] - bitAmount[s][c] == 0;
				tempConstraint.name = consName.str();
				problemSolver->addConstraint(tempConstraint);
			}
		}
	}

	void OptimalCompressionStrategy::generateConstraintC1(){
		const int LARGE_NUMBER = 10000;
		for(unsigned int s = 0; s < compCountVars.size(); s++){
			for(int c = 0; c < (int)bitAmount[s].size(); c++){
				stringstream consName;
				consName << "C1_" << s << "_" << c;
				ScaLP::Term c1Term;

				for(unsigned int e = 0; e < possibleCompressors.size(); e++){
					for(int ce = 0; ce < (int) possibleCompressors[e]->getHeights(); ce++){
						//REPORT(DEBUG, "ce is " << ce << " for compressor " << possibleCompressors[e]->getStringOfIO());
						if(c - ce >= 0){
							int tempColumn = possibleCompressors[e]->getHeights() - ce - 1;
							c1Term = c1Term + possibleCompressors[e]->getHeightsAtColumn((unsigned) tempColumn, true) * compCountVars[s][e][(unsigned)(c - ce)];
						}
					}
				}
				if(s != 0){
					//first stage N starts with s-1
					c1Term = c1Term - columnBitCountVars[s - 1][c];
				}
				//U
				//REPORT(DEBUG, "before U");
				c1Term = c1Term - newBitsCountVars[s][c];
				//REPORT(DEBUG, "after adding U");
				//Z
				c1Term = c1Term - emptyInputVars[s][c];
				c1Term = c1Term + LARGE_NUMBER * stageVars[s];
				ScaLP::Constraint c1Constraint = c1Term == 0;
				c1Constraint.name = consName.str();
				problemSolver->addConstraint(c1Constraint);
			}
		}

	}

	void OptimalCompressionStrategy::generateConstraintC2(){
		for(unsigned int s = 0; s < compCountVars.size(); s++){
			for(int c = 0; c < (int)bitAmount[s].size(); c++){
				stringstream consName;
				consName << "C2_" << s << "_" << c;
				ScaLP::Term c2Term;

				for(unsigned int e = 0; e < possibleCompressors.size(); e++){
					for(int ce = (int)possibleCompressors[e]->getOutHeights() - 1; ce >= 0; ce--){
						if(c - ce >= 0){
							int tempColumn = possibleCompressors[e]->getOutHeights() - ce - 1;
							c2Term = c2Term + possibleCompressors[e]->getOutHeightsAtColumn( (unsigned)tempColumn, true) * compCountVars[s][e][(unsigned) (c - ce)];
						}
					}
				}

				c2Term = c2Term - columnBitCountVars[s][c]; //columnbitCountvars starts at stage 1
				ScaLP::Constraint c2Constraint = c2Term == 0;
				c2Constraint.name = consName.str();
				problemSolver->addConstraint(c2Constraint);
			}
		}
	}

	void OptimalCompressionStrategy::generateConstraintC3(){
		const int LARGE_NUMBER = 10000;
		for(unsigned int s = 0; s < bitAmount.size(); s++){
			for(unsigned int c = 0; c < bitAmount[s].size(); c++){
				stringstream consName;
				consName << "C3_" << s << "_" << c;
				ScaLP::Term c3Term;

				if(s != 0){
					c3Term = c3Term + columnBitCountVars[s - 1][c];
				}
				c3Term = c3Term + newBitsCountVars[s][c];
				for(unsigned int z = s + 1; z < bitAmount.size(); z++){
					//make sure that that in the follwoing stages all the U's are empty.
					//this is done by adding all of the U-variables of later stages with the factor of 4. Therefore e.g. if s = 3 -> D_3 = 1 -> in constraint C3_3_c: 1000 + 4 * U_later <= 1002 -> if at least one U_later is >0, then constraint is not met
					// +4 instead of +3: if formulation is modiefied to a ternary adder, this constraint does not need to change change.
					c3Term = c3Term + 4 * newBitsCountVars[z][c];
				}
				c3Term = c3Term + LARGE_NUMBER * stageVars[s];
				ScaLP::Constraint c3Constraint = c3Term <= LARGE_NUMBER + 2;
				c3Constraint.name = consName.str();
				problemSolver->addConstraint(c3Constraint);

			}
		}
	}

	void OptimalCompressionStrategy::generateConstraintC4(){
		ScaLP::Term c4Term;
		for(unsigned int s = 0; s < stageVars.size(); s++){
			c4Term = c4Term + stageVars[s];
		}
		ScaLP::Constraint c4Constraint = c4Term - 1 == 0;
		c4Constraint.name = "C4";
		problemSolver->addConstraint(c4Constraint);
	}

	void OptimalCompressionStrategy::selectOutputStage(unsigned int stage){
		if(stage >= stageVars.size()){
			THROWERROR("tried to set an stage as outputstage which does not exist");
		}
		ScaLP::Constraint c4_1Constraint = stageVars[stage] - 1 == 0;
		c4_1Constraint.name = "C4_1";
		problemSolver->addConstraint(c4_1Constraint);
	}

	void OptimalCompressionStrategy::generateConstraintForVariableCompressors(){

	}

	bool OptimalCompressionStrategy::addFlipFlop(){

		bool foundFlipflop = false;
		for(unsigned int e = 0; e < possibleCompressors.size(); e++){
			//inputs
			if(possibleCompressors[e]->getHeights() == 1 && possibleCompressors[e]->getHeightsAtColumn(0) == 1){
				if(possibleCompressors[e]->getOutHeights() == 1 && possibleCompressors[e]->getOutHeightsAtColumn(0) == 1){
					foundFlipflop = true;
					flipflop = possibleCompressors[e];
					break;
				}
			}
		}

		if(!foundFlipflop){
			//add flipflop at back of possibleCompressor
			vector<int> newVect;
			BasicCompressor *newCompressor;
			int col0=1;
			newVect.push_back(col0);
			newCompressor = new BasicCompressor(bitheap->getOp(), bitheap->getOp()->getTarget(), newVect, 0.5, "combinatorial", true);
			possibleCompressors.push_back(newCompressor);

			flipflop = newCompressor;
		}

		return !foundFlipflop;
	}

	bool OptimalCompressionStrategy::solve(){

		for(unsigned int e = 0; e < possibleCompressors.size(); e++){
			REPORT(DEBUG, "at position " << e << " is compressor " << possibleCompressors[e]->getStringOfIO() << " with costs of " << possibleCompressors[e]->area);
		}

		bool solutionFound = false;
		problemSolver->threads = 1;
		problemSolver->quiet = false;

		REPORT(DEBUG, "backend while solving ilp problem is " << problemSolver->getBackendName());

		ScaLP::status stat = problemSolver->solve();

		if(stat == ScaLP::status::INFEASIBLE_OR_UNBOUND || stat == ScaLP::status::INFEASIBLE || stat == ScaLP::status::UNBOUND){
			solutionFound = false;
			REPORT(DEBUG, "problem is unbound, infeasible or no solution within timelimit is reached");
		}
		else if(stat == ScaLP::status::OPTIMAL || stat == ScaLP::status::FEASIBLE || stat == ScaLP::status::TIMEOUT_FEASIBLE ){
			solutionFound = true;
		}
		else // includes stat == ScaLP::status::TIMEOUT_INFEASIBLE)
        {
            THROWERROR("No feasible solution possible within given ILPTimeout.");
        }

		return solutionFound;
	}

	void OptimalCompressionStrategy::fillSolutionFromILP(){

		ScaLP::Result result = problemSolver->getResult();


		for(unsigned int s = 0; s < compCountVars.size(); s++){
			for(unsigned int e = 0; e < compCountVars[s].size(); e++){
				for(unsigned int c = 0; c < compCountVars[s][e].size(); c++){
					double tempValue = result.values[compCountVars[s][e][c]];
					tempValue += 0.00001;	//add small value
					int integerValue = (int) tempValue;
					if(integerValue > 0){
						if(possibleCompressors[e] != flipflop){
							for(unsigned int k = 0; k < (unsigned int) integerValue; k++){
								solution.addCompressor(s, c, possibleCompressors[e]);
							}
						}
					}
				}
			}
		}

		for(unsigned int s = 0; s < emptyInputVars.size(); s++){
			vector<int> tempVector;
			for(unsigned int c = 0; c < emptyInputVars[s].size(); c++){
				double tempValue = result.values[emptyInputVars[s][c]];
				tempValue += 0.00001;
				int integerValue = (int) tempValue;
				//the Z's in stage of the final adder have the value of ~ LARGE_NUMBER. Filter them because there are no holes there.
				if(integerValue < 100){
					tempVector.push_back(integerValue * (-1)); 	//the empty inputs must be negative.
				}
				else{
					tempVector.push_back(0);
				}
			}
			solution.setEmptyInputsByRemainingBits(s, tempVector);
		}
	}

	unsigned int OptimalCompressionStrategy::getMaxStageCount(){

		//catching the case that there is no bitheap needed
		if(daddaTwoBitStageReached(bitAmount[0], 0)){

			return 0;
		}
		unsigned int stages = 0;
		vector<int> tempVector(bitAmount[0].size());
		for(unsigned int c = 0; c < bitAmount[0].size(); c++){

			tempVector[c] = ceil((((float)bitAmount[0][c]) *  (2.0/3.0)) - 0.00001);
		}



		while(!daddaTwoBitStageReached(tempVector, stages)){
			stages++;
			if(stages < bitAmount.size()){
				for(unsigned int c = 0; c < bitAmount[stages].size(); c++){
					tempVector[c] += bitAmount[stages][c];
				}
			}
			for(unsigned int c = 0;c < bitAmount[0].size(); c++){
				tempVector[c] = ceil((((float)tempVector[c]) *  (2.0/3.0)) - 0.00001);
			}
		}

		stages++; //for first ceil
		return stages;
	}

	bool OptimalCompressionStrategy::daddaTwoBitStageReached(vector<int> currentBits, unsigned int stage){
		bool reached = true;
		for(unsigned w = 0; w < currentBits.size(); w++){
			if(currentBits[w] > 2){
				reached = false;
			}
		}

		//check for inputbits which arrive later
		for(unsigned int s = stage + 1; s < bitAmount.size(); s++){
			for(unsigned int c = 0; c < bitAmount[s].size(); c++){
				if(bitAmount[s][c] > 0){
					reached = false;
				}
			}
		}
		return reached;
	}

	unsigned int OptimalCompressionStrategy::getMinAmountOfStages(){

		//computes how many stages the compressor tree needs at least.
		//this is done by checking what is the highest stage where bits from outside arrive.
		//there can't be a compressortree, which is smaller than that highest stage

		//skip s == 0 because there must be bits in there
		for(unsigned int s = bitAmount.size() - 1; s > 0; s--){
			for(unsigned int c = 0; c < bitAmount[s].size(); c++){
				if(bitAmount[s][c] > 0){
					return s;
				}
			}
		}

		//we found no inputbits in later stages, therefore return 0;
		return 0;
	}


#endif //HAVE_SCALP
}
