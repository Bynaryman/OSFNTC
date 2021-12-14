This directory contains everything related to fixed-point multiplication.
Other operators will mostly use instances of
 IntMultiplier
 IntSquarer
 FixMultAdd
The other files in this directory are helper methods for building large multipliers out of smaller ones.
(description here?)

IntMultiplier and IntSquarer have two constructors:
  - a standard one, called from the command line, that builds a standalone operator
	- a bitheap-oriented one that input a bit heap and adds the multiplier/squarer to this bit heap.
	  A use case for this second constructor is FixMultAdd, which is why I consider fixing FixMultAdd also of high priority. Also, FixMultAdd is quite important for polynomial generators.



* TODOs (in F2D's order of importance)

** write a proper IntMultiplier::unitTest();
 
** same for FixMultAdd

** Manage properly the small multipliers (one threshold for very small -> LUT based, one threshold for small enough to fit in a DSP block). Same for FixMultAdd

** manage properly the signed/unsigned combination

** integrate latest tiling algorithms but have a sensible default fallback

** manage properly Xilinx and Altera targets.

** IntSquarer should be resurrected (FPLog depends on it, among others).
 It should have an optional wOut:
   - defaulting to 0, meaning full squarer
	 - -1 would mean "faithful to wIn"
	 - any positive value would mean "faithful to wOut"
 For small sizes, it can use plain tabulation.


