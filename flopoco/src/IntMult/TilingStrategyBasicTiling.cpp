#include <algorithm>

#include "TilingStrategyBasicTiling.hpp"
#include "IntMultiplier.hpp"

using namespace std;
namespace flopoco {
TilingStrategyBasicTiling::TilingStrategyBasicTiling(
		unsigned int wX_,
		unsigned int wY_,
		unsigned int wOut_,
		bool signedIO_,
		BaseMultiplierCollection* bmc,
		base_multiplier_id_t prefered_multiplier,
		float occupation_threshold,
		size_t maxPrefMult):TilingStrategy(
			wX_,
			wY_,
			wOut_,
			signedIO_,
			bmc),
		prefered_multiplier_{prefered_multiplier},
		small_tile_mult_{1}, //Most compact LUT-Based multiplier
		numUsedMults_{0},
		occupation_threshold_{occupation_threshold},
		max_pref_mult_{maxPrefMult}
	{
		truncated = ((unsigned)wOut < IntMultiplier::prodsize(wY, wX));
	}

	int TilingStrategyBasicTiling::shrinkBox(
			int& xright,
			int& ytop, 
			int& xdim, 
			int& ydim, 
			int offset)
	{
		// Come into the frame
		if (xdim + xright > wX) {
			xdim = wX - xright;
		}
		if (ydim + ytop > wY) {
			ydim = wY - ytop;
		}
		if (xright < 0) {
			xdim += xright;
			xright = 0;
		}
		if (ytop < 0) {
			ydim += ytop;
			ytop = 0;
		}

		// If all the the bits in the box are kept
		if (xright + ytop >= offset) {
			return xdim * ydim;
		}

		// If there is only truncated out bits in the box
		if (xright + xdim + ytop + ydim - 2 < offset) {
			xright = ytop = -1;
			xdim = ydim = 0;
			return 0;
		}

		int xinterLowRight = xright;
		int yinterLowRight = offset - xright;

		int yinterUpLeft = ytop;
		int xinterUpLeft = offset - ytop;

		if (yinterLowRight > ytop + ydim - 1) {
			xinterLowRight += yinterLowRight - (ytop + ydim - 1);
			yinterLowRight = ytop + ydim - 1;
		}

		if (xinterUpLeft > xright + xdim - 1) {
			yinterUpLeft += xinterUpLeft - (xright + xdim - 1);
			xinterUpLeft = xright + xdim - 1;
		}

		xdim -= (xinterLowRight - xright);
		ydim -= (yinterUpLeft - ytop);
		xright = xinterLowRight;
		ytop = yinterUpLeft;

		int untruncatedAreaFromTop = (xright + xdim - 1 - xinterUpLeft) * ydim;
		int untruncatedAreaLowRight = (xinterUpLeft + 1 - xright) * (ytop + ydim - 1 - yinterLowRight);
		int truncationTriangleDiag = (xinterUpLeft + 1 - xright);
		int truncatedArea = (truncationTriangleDiag * (truncationTriangleDiag + 1)) / 2;

		return truncatedArea + untruncatedAreaFromTop + untruncatedAreaLowRight;

		int curOffset = offset - xright - ytop;

		// If we are completely in the truncated part
		if (curOffset >= (xdim + ydim - 2)) {
			xright = ytop = -1;
			xdim = ydim = 0;
			return 0;
		}
		// Case where both the dimension have to be shrinked
		if (curOffset > xdim - 1 && curOffset > ydim - 1) {
			int verticeLen = (xdim + ydim) - curOffset;
			xright = xright + xdim - verticeLen;
			ytop = ytop + ydim - verticeLen;
			xdim = ydim = verticeLen;
			return (verticeLen * (verticeLen + 1)) >> 1;
		} else if (curOffset > xdim - 1) {
			// Skip uneeded empty rows
			int deltaY = curOffset - (xdim - 1);
			ytop += deltaY;
			ydim -= deltaY;
		} else if (curOffset > ydim - 1) {
			// Skip uneeded empty cols
			int deltaX = curOffset - (ydim- 1);
			xright += deltaX;
			xdim -= deltaX;
		}

		curOffset = max(offset - xright - ytop, 0);
		int area = xdim * ydim - (curOffset * (curOffset + 1))/2;
		return area;
	}

	float TilingStrategyBasicTiling::tileBox(
			int curX,
			int curY,
			int curDeltaX,
			int curDeltaY,
			int offset
		) 
	{
		auto& bmc = baseMultiplierCollection->getBaseMultiplier(small_tile_mult_);
		bool canOverflowLeft = not truncated and not signedIO and (curX + curDeltaX) >= wX;
		bool canOverflowRight = false;
		bool canOverflowTop = (curY == 0);
		bool canOverflowBottom = false;

		bool leftmostIsSignBit = (curX + curDeltaX >= wX) and signedIO ;
		bool bottomMostIsSignBit = (curY + curDeltaY >= wY) and signedIO;

		int xleft = curX + curDeltaX - 1;
		int ybottom = curY + curDeltaY - 1;
		int nbInputMult = bmc.getMaxWordSizeLargeInputUnsigned();
		int deltaWidthSign = bmc.getDeltaWidthSigned();
		if (leftmostIsSignBit) {
			nbInputMult += deltaWidthSign;
		}
		if(bottomMostIsSignBit) {
			nbInputMult += deltaWidthSign;
		}

		int bestXMult = 1;
		int bestYMult = 1;
		int bestArea = -1;
		int bestXAnchor = curX;
		int bestYAnchor = curY;

		float boxCost = 0.0f;

		for (int i = nbInputMult ; i > 1 ; --i) {
			int xMult, yMult;
			int xanchor, yanchor, xendmultbox, yendmultbox;
			for (xMult = nbInputMult - 1 ; xMult > 0 ; --xMult) {
				yMult = i - xMult;

				if (truncated) {
				xanchor = xleft - xMult + 1;
				yanchor = ybottom - yMult + 1;
				} else {
					xanchor = curX;
					yanchor = curY;	
				}
				xendmultbox = xanchor + xMult - 1;
				yendmultbox = yanchor + yMult - 1;

				// Check that the considered configuration is valid
				bool signedX = (xendmultbox >= wX - 1) and signedIO;
				bool signedY = (yendmultbox >= wY - 1) and signedIO;

				int maxY = bmc.getMaxSecondWordSize(xMult, signedX, signedY);
				if (yMult > maxY) {
					continue;
				}

				// Test if multiplier does not overflow on already handled data
				if (((yanchor < curY) && !canOverflowTop) ||
						((yendmultbox > ybottom) && !canOverflowBottom) ||
						((xanchor < curX) && !canOverflowRight) || 
						((xendmultbox > xleft) && !canOverflowLeft)) {
					continue;
				}

				int copyxanchor, copyyanchor, copyXMult, copyYMult;
				copyxanchor = xanchor;
				copyyanchor = yanchor;
				copyXMult = xMult;
				copyYMult = yMult;
				int effectiveArea = shrinkBox(
						copyxanchor, 
						copyyanchor, 
						copyXMult, 
						copyYMult, 
						offset
						);
				if (effectiveArea == xMult * yMult && effectiveArea > bestArea) {
					bestXMult = xMult;
					bestYMult = yMult;
					bestArea = effectiveArea;
					bestXAnchor = xanchor;
					bestYAnchor = yanchor;
				}
			}
		}

		int xendmultbox = bestXAnchor + bestXMult - 1;
		int yendmultbox = bestYAnchor + bestYMult - 1;

		bool signedX = (xendmultbox >= wX - 1) and signedIO;
		bool signedY = (yendmultbox >= wY - 1) and signedIO;
		
		if (bestYMult < curDeltaY) {
			//We need to tile a subbox above or below the current multiplier
			int xStartUpDownbox = curX;
			int deltaXUpDownbox = curDeltaX;
			int yStartUpDownbox, deltaYUpDownbox;
			if (truncated) {
				yStartUpDownbox = curY;
				deltaYUpDownbox = bestYAnchor - curY;
			} else {
				yStartUpDownbox = yendmultbox + 1;	
				deltaYUpDownbox = ybottom - yendmultbox;
			}

			int subboxArea = shrinkBox(
					xStartUpDownbox, 
					yStartUpDownbox, 
					deltaXUpDownbox, 
					deltaYUpDownbox, 
					offset
				);
			if (subboxArea != 0) {
				boxCost += tileBox(
						xStartUpDownbox, 
						yStartUpDownbox, 
						deltaXUpDownbox, 
						deltaYUpDownbox,
						offset
					);
			}
		}
		if (bestXMult < curDeltaX) {
			//we need to tile a subbox left or right from the multiplier
			int yStartUpDownbox = bestYAnchor;
			int deltaYUpDownbox = bestYMult;
			int xStartUpDownbox, deltaXUpDownbox;
			if (truncated) {
				xStartUpDownbox = curX;
				deltaXUpDownbox = bestXAnchor - curX;
			} else {
				xStartUpDownbox = xendmultbox + 1;	
				deltaXUpDownbox = xleft - xendmultbox;
			}

			int subboxArea = shrinkBox(
					xStartUpDownbox, 
					yStartUpDownbox, 
					deltaXUpDownbox, 
					deltaYUpDownbox, 
					offset
				);
			if (subboxArea != 0) {
				boxCost += tileBox(
						xStartUpDownbox, 
						yStartUpDownbox, 
						deltaXUpDownbox, 
						deltaYUpDownbox,
						offset
					);
			}
		}
		//Add the current multiplier
		auto param = bmc.parametrize(bestXMult, bestYMult, signedX, signedY);
		auto coord = make_pair(bestXAnchor, bestYAnchor);
		solution.push_back(make_pair(param, coord));
        boxCost += (float)bmc.getLUTCost(bestXAnchor, bestYAnchor, wX, wY);;
        return boxCost;
	}

	void TilingStrategyBasicTiling::solve()
	{
        float totalCost = 0.0f;
		auto& bm = baseMultiplierCollection->getBaseMultiplier(prefered_multiplier_);	
		int wXmultMax = bm.getMaxWordSizeLargeInputUnsigned();
		//TODO Signed int deltaSignedUnsigned = bm.getDeltaWidthSigned();
		int wXmult = 1;
		int wYmult = 1;
		int intMultArea = 1;
		for (int testValX = 1 ; testValX < wXmultMax ; ++testValX) {
			int testValY = bm.getMaxSecondWordSize(testValX, false, false);
			int testMultArea = testValX * testValY;
			if (testMultArea >= intMultArea) {
				wXmult = testValX;
				wYmult = testValY;
				intMultArea = testMultArea;
			}
		}

		int deltaWidthSigned = bm.getDeltaWidthSigned();

		float multArea = float(intMultArea);

		if (truncated) {
			//Perform tiling from left to right to increase the number of full
			//multipliers	
			int offset = IntMultiplier::prodsize(wX, wY) - wOut;
			int curX = wX - 1;
			int curY = 0;
			if (curX < offset) {
				curY = offset - curX;
			}

			while (curY < wY) {
				curX = wX - 1;
				bool isYSigned = ((static_cast<unsigned int>(curY + wYmult)) >= (unsigned)wY) and signedIO;
				while(curX >= 0) {
					int curDeltaX = wXmult;
					int curDeltaY = wYmult;
					bool isXSigned = ((static_cast<unsigned int>(curX + wXmult)) >= (unsigned)wX) and signedIO;

					if (isYSigned)
						curDeltaY += deltaWidthSigned;
					if (isXSigned)
						curDeltaX += deltaWidthSigned;

					int rightX = curX - curDeltaX + 1;
					int topY = curY;

					int area = shrinkBox(rightX, topY, curDeltaX, curDeltaY, offset);
					if (area == 0) { // All the box is below the line cut
						break;
					} 

					float occupationRatio = (float (area)) /  multArea;
					bool occupationAboveThreshold = occupationRatio >= occupation_threshold_;
					bool hardLimitUnreached = (numUsedMults_ < max_pref_mult_);
					if (occupationAboveThreshold and hardLimitUnreached) {
						// Emit a preferred multiplier for this block
						auto param = bm.parametrize(
								curDeltaX, 
								curDeltaY, 
								isXSigned,
								isYSigned
							);
						auto coords = make_pair(rightX, topY);
						solution.push_back(make_pair(param, coords));
						totalCost += 0.65 * param.getOutWordSize();
						numUsedMults_ += 1;
					} else {
						//Tile the subBox with smaller multiplier;
						totalCost += tileBox(rightX, topY, curDeltaX, curDeltaY, offset);
					}
					curX -= wXmult;
					if (isXSigned)
						curX -= deltaWidthSigned;
				}
				curY += wYmult;
				if (isYSigned)
					curY += deltaWidthSigned;
			}

		} else {
			//Perform tiling from right to left to avoid small multipliers for
			//low bits
			int curX = 0;
			int curY = 0;

			while (curY < wY) {
				curX = 0;
				int untilEndCol = wY - (wYmult + curY);
				while(curX < wX) {
					int rightX = curX;
					int topY = curY;
					int curDeltaX = wXmult;
					int curDeltaY = wYmult;

					int untilEndRow = wX - (curDeltaX + rightX);

					if((untilEndRow <= deltaWidthSigned) and signedIO) {
						curDeltaX += untilEndRow;
					}

					if ((untilEndCol <= deltaWidthSigned) and signedIO) {
						curDeltaY += untilEndCol;
					}

					int area = shrinkBox(rightX, topY, curDeltaX, curDeltaY, 0);
					if (area == 0) { // All the box is below the line cut
						break;
					} 

					bool isSignedX = (rightX + curDeltaX >= wX) and signedIO;
					bool isSignedY = (topY + curDeltaY >= wY) and signedIO;

					float occupationRatio = float(area) /  multArea;
					bool occupationAboveThreshold = occupationRatio >= occupation_threshold_;
					bool hardLimitUnreached = (numUsedMults_ < max_pref_mult_);
					if (occupationAboveThreshold and hardLimitUnreached) {
						// Emit a preferred multiplier for this block
						auto param = bm.parametrize(
								curDeltaX,
								curDeltaY,
								isSignedX,
								isSignedY
							);
						auto coord = make_pair(rightX, topY);
						solution.push_back(make_pair(param, coord));
						totalCost += 0.65 * param.getOutWordSize();
						numUsedMults_ += 1;
					} else {
						//Tile the subBox with smaller multiplier;
						totalCost += tileBox(rightX, topY, curDeltaX, curDeltaY, 0);
					}
					curX += wXmult;
					if ((untilEndRow <= deltaWidthSigned) and signedIO) {
						curX += untilEndRow;
					}
				}
				curY += wYmult;
				if ((untilEndCol <= deltaWidthSigned) and signedIO) {
					curY += untilEndCol;
				}
			}
		}

        cout << "Total cost: " << totalCost << endl;
	}
};
