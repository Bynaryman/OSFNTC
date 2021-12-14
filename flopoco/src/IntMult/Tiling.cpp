#include "Tiling.hpp"
using namespace std;


namespace flopoco{


		Tiling::Tiling(Target* target_, int wX_, int wY_, double ratio_, bool truncated_, int truncationSize_) :
			target(target_), wX(wX_), wY(wY_), ratio(ratio_), truncated(truncated_), truncationSize(truncationSize_)
		{
			if(truncated){
				xOrigin = wX-1;
				yOrigin = wY-1;
				
				xIncrement = -1;
				yIncrement = -1;
			}else{
				xOrigin = 0;
				yOrigin = 0;
				
				xIncrement = 1;
				yIncrement = 1;
			}
			
			xBorder = wX - xOrigin - 1;
			yBorder = wY - yOrigin - 1;
		}
		
		Tiling::~Tiling();

		vector<MultiplierBlock*> Tiling::createTiling()
		{
			vector<MultiplierBlock*> hTiling, vTiling, mixedTiling, bestConfiguration;
			
			hTiling = createHorizontalTiling();
			vTiling = createVerticalTiling();
			mixedTiling = createMixedTiling();
			
			bestConfiguration = hTiling;
			if(computeTilingCost(vTiling) > computeTilingCost(bestConfiguration))
				bestConfiguration = vTiling;
			if(computeTilingCost(mixedTiling) > computeTilingCost(bestConfiguration))
				bestConfiguration = mixedTiling;
				
			return bestConfiguration;
		}
		
		vector<MultiplierBlock*> Tiling::createHorizontalTiling()
		{
			vector<MultiplierBlock*> result;
			int xCurrent, yCurrent;
			int dspWidth, dspHeight;
			bool tilingIncomplete = true;
			
			//get the width and height of a DSP
			getTarget()->getDSPWidths(dspWidth, dspHeight);
			//make sure the tile is placed horizontally; exchange values if necessary
			if(dspHeight>dspWidth){
				dspHeight += dspWidth;
				dspWidth = dspHeight - dspWidth;
				dspHeight = dspHeight - dspWidth;
			}
			
			//start tiling, one line at a time, from the origin towards the 
			//	interior of the board
			xCurrent = xOrigin;
			yCurrent = yOrigin;
			while((yBorder-yCurrent)*yIncrement > 0)
			{
				//tile a row until the last tile is either on the edge of the 
				//	board or has surpassed it
				while((xBorder-xCurrent)*xIncrement > 0){
					MultiplierBlock* newMultiplier = new MultiplierBlock(dspWidth, dspHeight, xCurrent, yCurrent);
					result.push_back(newMultiplier);
					
					xCurrent += xIncrement * dspWidth;
				}
				
				//if the last tile is outside, decide if it should be kept, or 
				//	replaced with a logic-only multiplier, depending on the ratio
				if((xBorder-xCurrent*xIncrement) < 0){
					//if the percentage of unused area in the last DSP is larger 
					//	than what is allowed by ratio, then replace it
					if((xCurrent-xBorder)*xIncrement*1.0/dspWidth > (1.0-ratio)){
						result.pop_back();
						MultiplierBlock* newMultiplier = new MultiplierBlock((xBorder-(xCurrent-xIncrement*dspWidth))*xIncrement, dspHeight, xCurrent-xIncrement*dspWidth, yCurrent, false);
						result.push_back(newMultiplier);
					}
				}
				
				//pass to the next row
				yCurrent += yIncrement * dspHeight;
				xCurrent = xOrigin;
				
				//if this is the last row, all tiles might need to be implemented 
				//	in logic, depending on how much of them is being used
				if(abs(yBorder-yCurrent) < dspHeight){
					int remainingHeight;
				
					remainingHeight = (truncated) ? yCurrent :  yBorder-yCurrent;
					
					MultiplierBlock* newMultiplier = new MultiplierBlock(wX, remainingHeight, (truncated ? xOrigin : xBorder), (truncated ? yCurrent : yBorder), false);
					result.push_back(newMultiplier);
					
					break();
				}
				
			}
		}
		
		vector<MultiplierBlock*> Tiling::createVerticalTiling()
		{
			vector<MultiplierBlock*> result;
			int xCurrent, yCurrent;
			int dspWidth, dspHeight;
			bool tilingIncomplete = true;
			
			//get the width and height of a DSP
			getTarget()->getDSPWidths(dspWidth, dspHeight);
			//make sure the tile is placed horizontally; exchange values if necessary
			if(dspHeight<dspWidth){
				dspHeight += dspWidth;
				dspWidth = dspHeight - dspWidth;
				dspHeight = dspHeight - dspWidth;
			}
			
			//start tiling, one column at a time, from the origin towards the 
			//	interior of the board
			xCurrent = xOrigin;
			yCurrent = yOrigin;
			while((xBorder-xCurrent)*xIncrement > 0)
			{
				//tile a row until the last tile is either on the edge of the 
				//	board or has surpassed it
				while((yBorder-yCurrent)*yIncrement > 0){
					MultiplierBlock* newMultiplier = new MultiplierBlock(dspWidth, dspHeight, xCurrent, yCurrent);
					result.push_back(newMultiplier);
					
					yCurrent += yIncrement * dspHeight;
				}
				
				//if the last tile is outside, decide if it should be kept, or 
				//	replaced with a logic-only multiplier, depending on the ratio
				if((yBorder-yCurrent*yIncrement) < 0){
					//if the percentage of unused area in the last DSP is larger 
					//	than what is allowed by ratio, then replace it
					if((yCurrent-yBorder)*yIncrement*1.0/dspHeight > (1.0-ratio)){
						result.pop_back();
						MultiplierBlock* newMultiplier = new MultiplierBlock(dspWidth, (yBorder-(yCurrent-yIncrement*dspHeight))*yIncrement, xCurrent, yCurrent-yIncrement*dspHeight, false);
						result.push_back(newMultiplier);
					}
				}
				
				//pass to the next row
				xCurrent += xIncrement * dspWidth;
				yCurrent = yOrigin;
				
				//if this is the last column, all tiles might need to be implemented 
				//	in logic, depending on how much of them is being used
				if(abs(xBorder-xCurrent) < dspWidth){
					int remainingWidth;
				
					remainingWidth = (truncated) ? xCurrent :  xBorder-xCurrent;
					
					MultiplierBlock* newMultiplier = new MultiplierBlock(remainingWidth, wY, (truncated ? xOrigin : xCurrent), (truncated ? yOrigin : yBorder), false);
					result.push_back(newMultiplier);
					
					break();
				}
				
			}
		}
		
		vector<MultiplierBlock*> Tiling::createMixedTiling()
		{
			cerr << "Function createMixedTiling not yet implemented. Exiting." << endl;
			exit(1);
		}
		
		
		double Tiling::computeTilingCost(vector<MultiplierBlock*> configuration)
		{
			cerr << "Function computeTilingCost not yet implemented. Exiting." << endl;
			exit(1);
		}
		
		
		bool Tiling::validateTiling(vector<MultiplierBlock*> configuration)
		{
			cerr << "Function validateTiling not yet implemented. Exiting." << endl;
			exit(1);
		}
}
