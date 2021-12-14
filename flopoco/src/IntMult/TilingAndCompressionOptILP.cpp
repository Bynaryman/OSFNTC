#include "IntMult/TilingAndCompressionOptILP.hpp"

#include "BaseMultiplierLUT.hpp"
#include "MultiplierTileCollection.hpp"

using namespace std;
namespace flopoco {

TilingAndCompressionOptILP::TilingAndCompressionOptILP(
		unsigned int wX_,
		unsigned int wY_,
		unsigned int wOut_,
		bool signedIO_,
		BaseMultiplierCollection* bmc,
		base_multiplier_id_t prefered_multiplier,
		float occupation_threshold,
		size_t maxPrefMult,
        MultiplierTileCollection mtc_,
        BitHeap* bitheap):TilingStrategy(
			wX_,
			wY_,
			wOut_,
			signedIO_,
			bmc),
        CompressionStrategy(bitheap),
		small_tile_mult_{1}, //Most compact LUT-Based multiplier
		numUsedMults_{0},
		max_pref_mult_ {maxPrefMult},
		occupation_threshold_{occupation_threshold},
		tiles{mtc_.MultTileCollection}
	{
        for(auto &p:tiles)
        {
                cout << p->getLUTCost(0, 0, wX, wY) << " " << p->getType() << endl;
        }
 	}

void TilingAndCompressionOptILP::solve()
{

#ifndef HAVE_SCALP
    throw "Error, TilingAndCompressionOptILP::solve() was called but FloPoCo was not built with ScaLP library";
#else
    cout << "using ILP solver " << target->getILPSolver() << endl;
    solver = new ScaLP::Solver(ScaLP::newSolverDynamic({target->getILPSolver(),"Gurobi","CPLEX","SCIP","LPSolve"}));
    solver->timeout = target->getILPTimeout();

    ScaLP::status stat;
    int s_max = 0;
    do{
        s_max++;
        solver->reset();
        constructProblem(s_max);

        // Try to solve
        cout << "starting solver, this might take a while..." << endl;
        solver->quiet = false;
        stat = solver->solve();

        // print results
        cerr << "The result is " << stat << endl;
        //cerr << solver->getResult() << endl;
    } while(stat == ScaLP::status::INFEASIBLE);

    ScaLP::Result res = solver->getResult();

    double total_cost = 0;
    int dsp_cost = 0;
    for(auto &p:res.values)
    {
        if(p.second > 0.5){     //parametrize all multipliers at a certain position, for which the solver returned 1 as solution, to flopoco solution structure
            std::string var_name = p.first->getName();
            cout << var_name << "\t " << p.second << endl;
            if(var_name.substr(1,1).compare("d") != 0) continue;
            int mult_id = stoi(var_name.substr(2,dpS));
            int x_negative = (var_name.substr(2+dpS,1).compare("m") == 0)?1:0;
            int m_x_pos = stoi(var_name.substr(2+dpS+x_negative,dpX)) * ((x_negative)?(-1):1);
            int y_negative = (var_name.substr(2+dpS+x_negative+dpX,1).compare("m") == 0)?1:0;
            int m_y_pos = stoi(var_name.substr(2+dpS+dpX+x_negative+y_negative,dpY)) * ((y_negative)?(-1):1);
            cout << "is true:  " << setfill(' ') << setw(dpY) << mult_id << " " << setfill(' ') << setw(dpY) << m_x_pos << " " << setfill(' ') << setw(dpY) << m_y_pos << " cost: " << setfill(' ') << setw(5) << tiles[mult_id]->getLUTCost(m_x_pos, m_y_pos, wX, wY) << std::endl;

            total_cost += (double)tiles[mult_id]->getLUTCost(m_x_pos, m_y_pos, wX, wY);
            dsp_cost += (double)tiles[mult_id]->getDSPCost();
            auto coord = make_pair(m_x_pos, m_y_pos);
            TilingStrategy::solution.push_back(make_pair(tiles[mult_id]->getParametrisation().tryDSPExpand(m_x_pos, m_y_pos, wX, wY, signedIO), coord));

        }
    }
    cout << "Total LUT cost:" << total_cost <<std::endl;
    cout << "Total DSP cost:" << dsp_cost <<std::endl;
/*
    solution.push_back(make_pair(tiles[1]->getParametrisation().tryDSPExpand(0, 0, wX, wY, signedIO), make_pair(0, 0)));
    solution.push_back(make_pair(tiles[0]->getParametrisation().tryDSPExpand(16, 0, wX, wY, signedIO), make_pair(16, 0)));
    solution.push_back(make_pair(tiles[0]->getParametrisation().tryDSPExpand(32, 0, wX, wY, signedIO), make_pair(32, 0)));
    solution.push_back(make_pair(tiles[0]->getParametrisation().tryDSPExpand(0, 24, wX, wY, signedIO), make_pair(0, 24)));
    solution.push_back(make_pair(tiles[0]->getParametrisation().tryDSPExpand(16, 24, wX, wY, signedIO), make_pair(16, 24)));
    solution.push_back(make_pair(tiles[0]->getParametrisation().tryDSPExpand(32, 24, wX, wY, signedIO), make_pair(32, 24)));
    solution.push_back(make_pair(tiles[0]->getParametrisation().tryDSPExpand(48, 24, wX, wY, signedIO), make_pair(48, 24)));
    solution.push_back(make_pair(tiles[0]->getParametrisation().tryDSPExpand(16, 48, wX, wY, signedIO), make_pair(16, 48)));
    solution.push_back(make_pair(tiles[0]->getParametrisation().tryDSPExpand(32, 48, wX, wY, signedIO), make_pair(32, 48)));
*/
//    solution.push_back(make_pair(tiles[1]->getParametrisation(), make_pair(0, 0)));

#endif
}

#ifdef HAVE_SCALP
void TilingAndCompressionOptILP::constructProblem(int s_max)
{
    cout << "constructing problem formulation..." << endl;
    wS = tiles.size();


    //Assemble cost function, declare problem variables
    cout << "   assembling cost function, declaring problem variables..." << endl;
    ScaLP::Term obj;
    int x_neg = 0, y_neg = 0;
    for(int s = 0; s < wS; s++){
        x_neg = (x_neg < (int)tiles[s]->wX())?tiles[s]->wX() - 1:x_neg;
        y_neg = (y_neg < (int)tiles[s]->wY())?tiles[s]->wY() - 1:y_neg;
    }
    int nx = wX-1, ny = wY-1, ns = wS-1; dpX = 1; dpY = 1; dpS = 1; //calc number of decimal places, for var names
    nx = (x_neg > nx)?x_neg:nx;                                     //in case the extend in negative direction is larger
    ny = (y_neg > ny)?y_neg:ny;
    while (nx /= 10)
        dpX++;
    while (ny /= 10)
        dpY++;
    while (ns /= 10)
        dpS++;

    vector<ScaLP::Term> bitsinColumn(wX+wY);
    vector<vector<vector<ScaLP::Variable>>> solve_Vars(wS, vector<vector<ScaLP::Variable>>(wX+x_neg, vector<ScaLP::Variable>(wY+y_neg)));
    ScaLP::Term maxEpsTerm;
    __uint64_t sumOfPosEps = 0;
    // add the Constraints
    cout << "   adding the constraints to problem formulation..." << endl;
    for(int y = 0; y < wY; y++){
        for(int x = 0; x < wX; x++){
            stringstream consName;
            consName << "p" << setfill('0') << setw(dpX) << x << setfill('0') << setw(dpY) << y;            //one constraint for every position in the area to be tiled
            ScaLP::Term pxyTerm;
            for(int s = 0; s < wS; s++){					//for every available tile...
                for(int ys = 0 - tiles[s]->wY() + 1; ys <= y; ys++){					//...check if the position x,y gets covered by tile s located at position (xs, ys) = (x-wtile..x, y-htile..y)
                    for(int xs = 0 - tiles[s]->wX() + 1; xs <= x; xs++){
                        if(occupation_threshold_ == 1.0 && ((wX - xs) < (int)tiles[s]->wX() || (wY - ys) < (int)tiles[s]->wY())) break;
                        if(tiles[s]->shape_contribution(x, y, xs, ys, wX, wY, signedIO) == true){
                            if(tiles[s]->shape_utilisation(xs, ys, wX, wY, signedIO) >=  occupation_threshold_ ){
                                if(solve_Vars[s][xs+x_neg][ys+y_neg] == nullptr){
                                    stringstream nvarName;
                                    nvarName << " d" << setfill('0') << setw(dpS) << s << ((xs < 0)?"m":"") << setfill('0') << setw(dpX) << ((xs<0)?-xs:xs) << ((ys < 0)?"m":"")<< setfill('0') << setw(dpY) << ((ys<0)?-ys:ys) ;
                                    //std::cout << nvarName.str() << endl;
                                    ScaLP::Variable tempV = ScaLP::newBinaryVariable(nvarName.str());
                                    solve_Vars[s][xs+x_neg][ys+y_neg] = tempV;
                                    obj.add(tempV, (double)tiles[s]->ownLUTCost(xs, ys, wX, wY));    //append variable to cost function

                                    int col_min = xs+ys+tiles[s]->getRelativeResultLSBWeight(tiles[s]->getParametrisation());
                                    int col_max = xs+ys+tiles[s]->getRelativeResultMSBWeight(tiles[s]->getParametrisation());
                                    for(col_min = ((col_min < 0)?0:col_min); col_min < (((wX+wY)<col_max)?wX+wY:col_max); col_min++){
                                        //cout << "position " << col_min << endl;
                                        bitsinColumn[col_min].add(tempV, 1);
                                    }

                                    if(wOut < wX+wY && ((unsigned long)1<<(x+y)) <= ((unsigned long)1<<(wX+wY-wOut))){
                                        maxEpsTerm.add(solve_Vars[s][xs+x_neg][ys+y_neg], ((unsigned long)1<<(x+y)));
                                        //maxEpsTerm.add(solve_Vars[s][xs+x_neg][ys+y_neg], 1);
                                    }
                                }
                                pxyTerm.add(solve_Vars[s][xs+x_neg][ys+y_neg], 1);
                            }
                        }
                    }
                }
            }
            ScaLP::Constraint c1Constraint;
            if(wOut < wX+wY && ((unsigned long)1<<(x+y)) <= ((unsigned long)1<<(wX+wY-wOut-1))){
                c1Constraint = pxyTerm <= (bool)1;
                sumOfPosEps += ((unsigned long)1<<(x+y));
                cout << sumOfPosEps << " " << ((unsigned long)1<<(x+y)) << endl;
            } else {
                c1Constraint = pxyTerm == (bool)1;
            }

            c1Constraint.name = consName.str();
            solver->addConstraint(c1Constraint);
        }
    }

    //limit use of shape n
    int nDSPTiles = 0;
    for(int s = 0; s < wS; s++)
        if(tiles[s]->getDSPCost())
            nDSPTiles++;
    if(nDSPTiles) {
        cout << "   adding the constraint to limit the use of DSP-Blocks to " << max_pref_mult_ << " instances..." << endl;
        stringstream consName;
        consName << "limDSP";
        ScaLP::Term pxyTerm;
        for (int y = 0 - 24 + 1; y < wY; y++) {
            for (int x = 0 - 24 + 1; x < wX; x++) {
                for (int s = 0; s < wS; s++)
                    if (solve_Vars[s][x + x_neg][y + y_neg] != nullptr)
                        for (int c = 0; c < tiles[s]->getDSPCost(); c++)
                            pxyTerm.add(solve_Vars[s][x + x_neg][y + y_neg], 1);
            }
        }
        ScaLP::Constraint c1Constraint = pxyTerm <= max_pref_mult_;     //set max usage equ.
        c1Constraint.name = consName.str();
        solver->addConstraint(c1Constraint);
    }

    //make shure the available precision is present in case of truncation
    if(wOut < wX+wY){
        cout << "   multiplier is truncated by " << wX+wY-wOut << " bits, ensure sufficient precision..." << endl;
        cout << sumOfPosEps << " " << ((unsigned long)1<<(wX+wY-wOut-1));
        ScaLP::Constraint truncConstraint = maxEpsTerm >= (sumOfPosEps-((unsigned long)1<<(wX+wY-wOut-1)));
        //ScaLP::Constraint truncConstraint = maxEpsTerm >= (bool)1;
        stringstream consName;
        consName << "maxEps";
        truncConstraint.name = consName.str();
        solver->addConstraint(truncConstraint);
    }

    addFlipFlop();      //Add FF to list of compressors
    vector<vector<ScaLP::Variable>> bitsInColAndStage(s_max, vector<ScaLP::Variable>(bitsinColumn.size()));
    //ScaLP::Term selectLastStage;
    for(int s = 0; s < s_max; s++){
        vector<ScaLP::Term> bitsinNextColumn(wX+wY);
        vector<ScaLP::Term> bitsinCurrentColumn(wX+wY);
        vector<ScaLP::Term> bitsinLastStageColumn(wX+wY);
/*        stringstream stage;                 //
        stage << "D_" << s;
        ScaLP::Variable stageV = ScaLP::newBinaryVariable(stage.str());
        selectLastStage.add(stageV, 1);*/
        for(unsigned c = 0; c < bitsinColumn.size(); c++){
            for(unsigned e = 0; e < possibleCompressors.size(); e++){
                if(s < s_max - 1){
                    stringstream nvarName;
                    nvarName << "k_" << s << "_" << e << "_" << c;
                    //std::cout << nvarName.str() << endl;
                    ScaLP::Variable tempV = ScaLP::newIntegerVariable(nvarName.str(), 0, ScaLP::INF());
                    obj.add(tempV,  possibleCompressors[e]->area);    //append variable to cost function
                    for(int ce = 0; ce < (int) possibleCompressors[e]->getHeights() && ce < (int)bitsinCurrentColumn.size() - (int)c; ce++){   //Bits that can be removed by compressor e in stage s in column c for constraint C1
                        //cout << possibleCompressors[e]->getHeightsAtColumn((unsigned) ce, false) << " c: " << c+ce << endl;
                        bitsinCurrentColumn[c+ce].add(tempV, possibleCompressors[e]->getHeightsAtColumn((unsigned) ce, false));
                    }
                    for(int ce = 0; ce < (int) possibleCompressors[e]->getOutHeights() && ce < (int)bitsinNextColumn.size() - (int)c; ce++){   //Bits that can be removed by compressor e in stage s in column c for constraint C1
                        //cout << possibleCompressors[e]->getOutHeightsAtColumn((unsigned) ce, false) << " c: " << c+ce << endl;
                        bitsinNextColumn[c+ce].add(tempV, possibleCompressors[e]->getOutHeightsAtColumn((unsigned) ce, false));
                    }
                }
            }
            stringstream curBits;
            if(bitsInColAndStage[s][c] == nullptr){                                                                 //N_s_c: Bits that enter current compressor stage
                curBits << "N_" << s << "_" << c;
                //cout << curBits.str() << endl;
                bitsInColAndStage[s][c] = ScaLP::newIntegerVariable(curBits.str(), 0, ScaLP::INF());
            }
            if(s == 0){
                stringstream consName0;
                consName0 << "C0_" << s << "_" << c;
                bitsinColumn[c].add(bitsInColAndStage[s][c], -1);      //Output bits from sub-multipliers
                ScaLP::Constraint c0Constraint = bitsinColumn[c] == 0;     //C0_s_c
                c0Constraint.name = consName0.str();
                solver->addConstraint(c0Constraint);
            }
            if(s < s_max - 1){
                stringstream consName1, consName2, zeroBits, nextBits;
                consName1 << "C1_" << s << "_" << c;
                zeroBits << "Z_" << s << "_" << c;
                bitsinCurrentColumn[c].add(ScaLP::newIntegerVariable(zeroBits.str(), 0, ScaLP::INF()), -1);      //Unused compressor input bits, that will be set zero
                bitsinCurrentColumn[c].add(bitsInColAndStage[s][c], -1);      //Bits arriving in current stage of the compressor tree
                ScaLP::Constraint c1Constraint = bitsinCurrentColumn[c] == 0;     //C1_s_c
                c1Constraint.name = consName1.str();
                solver->addConstraint(c1Constraint);
                consName2 << "C2_" << s << "_" << c;
                if(bitsInColAndStage[s+1][c] == nullptr){
                    nextBits << "N_" << s+1 << "_" << c;
                    //cout << nextBits.str() << endl;
                    bitsInColAndStage[s+1][c] = ScaLP::newIntegerVariable(nextBits.str(), 0, ScaLP::INF());
                }
                bitsinNextColumn[c].add(bitsInColAndStage[s+1][c], -1); //Output Bits of compressors to next stage
                ScaLP::Constraint c2Constraint = bitsinNextColumn[c] == 0;     //C2_s_c
                c2Constraint.name = consName2.str();
                solver->addConstraint(c2Constraint);
            }
            if(s == s_max-1){
                stringstream consName3;
                consName3 << "C3_" << s << "_" << c;
                //bitsinLastStageColumn[c].add(stageV, 10000);
                bitsinLastStageColumn[c].add(bitsInColAndStage[s][c], 1);
                //if(s < s_max - 1)
                //    bitsinLastStageColumn[c].add(bitsInColAndStage[s+1][c], 4);
                ScaLP::Constraint c3Constraint = bitsinLastStageColumn[c] <= 2;     //C3_s_c
                c3Constraint.name = consName3.str();
                solver->addConstraint(c3Constraint);
                //cout << consName3.str() << " " << stage.str() << endl;
            }

        }
    }
 /*   ScaLP::Constraint c4Constraint = selectLastStage == 1;     //C4
    c4Constraint.name = "C4";
    solver->addConstraint(c4Constraint);
*/
    // Set the Objective
    cout << "   setting objective (minimize cost function)..." << endl;
    solver->setObjective(ScaLP::minimize(obj));

    // Write Linear Program to file for debug purposes
    cout << "   writing LP-file for debuging..." << endl;
    solver->writeLP("tile.lp");
}


#endif

    bool TilingAndCompressionOptILP::addFlipFlop(){
        //BasicCompressor* flipflop;
        bool foundFlipflop = false;
        for(unsigned int e = 0; e < possibleCompressors.size(); e++){
            //inputs
            if(possibleCompressors[e]->getHeights() == 1 && possibleCompressors[e]->getHeightsAtColumn(0) == 1){
                if(possibleCompressors[e]->getOutHeights() == 1 && possibleCompressors[e]->getOutHeightsAtColumn(0) == 1){
                    foundFlipflop = true;
                    //flipflop = possibleCompressors[e];
                    break;
                }
            }
        }

        if(!foundFlipflop){
            //add flipflop at back of possibleCompressor
            //vector<int> newVect;
            //BasicCompressor *newCompressor;
            //int col0=1;
            //newVect.push_back(col0);
            BasicCompressor *newCompressor = new BasicCompressor(bitheap->getOp(), bitheap->getOp()->getTarget(), vector<int> {1}, 0.5, "combinatorial", true);
            possibleCompressors.push_back(newCompressor);

            //flipflop = newCompressor;
        }

        return !foundFlipflop;
    }

    void TilingAndCompressionOptILP::compressionAlgorithm() {

    }
}   //end namespace flopoco
