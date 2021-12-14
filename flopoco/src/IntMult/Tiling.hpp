#ifndef Tiling_HPP
#define Tiling_HPP
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "../Target.hpp"
#include "../Operator.hpp"
#include "./MultiplierBlock.hpp"

namespace flopoco {

	/**
	 * Creates a tiling for a multiplication. The multiplication can be truncated 
	 * or not. If the multiplication is truncated, then the tiling begins in the 
	 * bottom left corner; if the multiplication is full, the tiling starts in 
	 * the top right corner. 
	 * The tiling also has a ratio on the DSP usage: for a solution to be valid, 
	 * the DSP block must have a utilization above the one given as parameter.
	 * The origin of the board should be in the bottom left corner.
	 */
	class Tiling
	{
	public: 
	
		/**
		 * The default constructor for the Tiling class
		 */
		Tiling(Target* target_, int wX_, int wY_, double ratio_, bool truncated_ = false, int truncationSize_);
		
		/**
		 * Tiling class' destructor
		 */
		~Tiling();
	
		
		/**
		 * Creates the tiling for the board with the dimensions x=wX and y=wY. 
		 * The result of the tiling is a vector of multiplier blocks that 
		 * represent sub-multiplications, implemented either in logic or by a DSP.
		 * @return the tiling of the board with blocks of fixed size (the DSPs)
		 * and with blocks of variable size
		 */
		vector<MultiplierBlock*> createTiling();
		
		/**
		 * Creates a tiling where all the tiles are placed horizontally.
		 * NOTE: when the DSPs are symmetric, does the same thing as 
		 * the createVerticalTiling() function
		 * @return the tiling of the board with blocks of fixed size (the DSPs)
		 * and with blocks of variable size
		 */
		vector<MultiplierBlock*> createHorizontalTiling();
		
		/**
		 * Creates a tiling where all the tiles are placed vertically.
		 * NOTE: when the DSPs are symmetric, does the same thing as 
		 * the createHorizontalTiling() function
		 * @return the tiling of the board with blocks of fixed size (the DSPs)
		 * and with blocks of variable size
		 */
		vector<MultiplierBlock*> createVerticalTiling();
		
		/**
		 * Creates a tiling where the tiles are placed vertically and horizontally
		 * @return the tiling of the board with blocks of fixed size (the DSPs)
		 * and with blocks of variable size
		 */
		vector<MultiplierBlock*> createMixedTiling();
		
		
		/**
		 * Compute the cost of the solution, based on the relative cost of a DSP 
		 * and of a LUT/slice.
		 * @param configuration the solution for the tiling
		 * @return the cost of the tiling
		 */
		double computeTilingCost(vector<MultiplierBlock*> configuration);
		
		
		/**
		 * Check if a tiling, given as parameter, is valid
		 * @param configuration the solution for the tiling
		 * @return whether the tiling is valid, or not
		 */
		bool validateTiling(vector<MultiplierBlock*> configuration);

		
	private:
	
		Target* op;
		int wX; 							/**< size of the board horizontally */
		int wY; 							/**< size of the board vertically */
		bool truncated;						/**< the multiplier is truncated, or not */
		int truncationSize;					/**< the bits with weights lower than this size are truncated */
		double ratio;						/**< the DSP utilization ratio */
		
		int xOrigin, yOrigin;				/**< the coordinates of the tiling departure point */
		int xBorder, yBorder;				/**< the coordinates of the point which is the oposite corner to the origin */
		int xIncrement, yIncrement;			/**< the increment for advancing the tiling; it can be both negative and positive, depending on whether the multiplier is truncated or not */
	};

}
#endif
