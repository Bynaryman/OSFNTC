#ifndef POSITUTILS_HPP
#define POSITUTILS_HPP

#include "utils.hpp"

namespace flopoco {

	typedef enum PD_TYPE {POSIT_MEMORY, POSIT_MULT} PD_TYPE;

	/**
	 * @brief compute the number of bits needed to represent the internal fields of a posit
	 * @param[in] {const int} the posit width
	 * @param[in] {const int} the tunable exponent size
	 * @param[in|out] {int} the number of bit needed to represent the scale (regime+exp)
	 * @param[in|out] {int} the number of bit needed to represent the mantissa
	 */
	void get_pd_components_width(const int, const int, int &, int &);

	/**
	 * @brief compute the number of bits needed to represent a PD as a bus
	 * @param[in] {const int} the posit width
	 * @param[in] {const int} the tunable exponent size
	 * @param[in] {const PD_TYPE} the type of PD
	 * @return {int} the number of bits needed to represent a PD as a bus
	 */
	int get_pd_size(const int, const int, const PD_TYPE);

	/**
	  * @brief compute the exponent BIAS for a posit configuration
	  *   TODO(lledoux): understand if +1 or +0
	  * @param[in] {const int} the posit width
	  * @param[in] {const int} the tunable exponent size
	  * @param[in] {const int} TODO remove : get rid of that, it is a temp to evaluate if +1 or +0
	  */
	int get_BIAS(const int, const int, int p1);

	void parse_arithmetic(vector<string> *, int &, int &, int &, bool&, int&);

}
#endif  // POSITUTILS_HPP

