#ifndef TILINGSTRATEGYBASICTILING_HPP
#define TILINGSTRATEGYBASICTILING_HPP

#include "TilingStrategy.hpp"

namespace flopoco {

class TilingStrategyBasicTiling : public TilingStrategy {
	public:
		TilingStrategyBasicTiling(
				unsigned int wX,
				unsigned int wY,
				unsigned int wOut,
				bool signedIO,
				BaseMultiplierCollection* bmc,
				base_multiplier_id_t prefered_multiplier,
				float occupation_threshold,
				size_t maxPrefMult=0);
		void solve();

	private:
		/**
		 * @brief Shrink box to only focus on the useful part of multiplier plan
		 * @param xright x coordinate of the right vertice of the box
		 * @param ytop y coordinate of the top vertice of the box
		 * @param xdim width of the box
		 * @param ydim height of the box
		 * @param number of bits to discard from the output
		 * @return The area of the new box (coordinates and dimension are updated in the parameter)
		 */
		int shrinkBox(int& xright, int& ytop, int& xdim, int& ydim, int offset);


		float tileBox(int curX, int curY, int curDeltaX, int curDeltaY, int offset);

		base_multiplier_id_t prefered_multiplier_;
		base_multiplier_id_t small_tile_mult_;
		size_t numUsedMults_;
		float occupation_threshold_; //TODO Add to constructor
		size_t max_pref_mult_;
		bool truncated;
};

}

#endif
