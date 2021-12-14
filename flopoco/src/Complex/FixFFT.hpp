#ifndef FIX_COMPLEX_KCM_HPP
#define FIX_COMPLEX_KCM_HPP
#include <vector>
#include <string>

#include <Operator.hpp>

using namespace std;

/* Radix-2 FFT
   A n=2^k points FFT is represented as a bidimentional array of size (k+1, n);
   each cell represents a signal. The first layer is the input, and the layer
   k is the output.


   Radix-4 FFT
   A n=4^k poinst FFT is represented as a bidimensional array of size (k+1,n);
   each cell represents a signal. The first layer is the input, and the layer k
   is the output.

   What I'm trying to achieve :
   Precision -> Dimension -> Components -> Costs
   What is easy ?
   Dimension <-> Components
   Components -> Costs
   Dimension -> Precision
 */

int uplog2(int);

namespace flopoco {
	
	/**
	 * @brief a Fix FFT in FloPoCo
	 */	
	class FixFFT : public Operator{
	public:
		/**
		 * @brief The type of a component
		 */
		typedef enum
			{ Trunc, /**< a truncature, top of the butterfly */
			  Exact, /**< an exact multiplication (4th roots of unit) */
			  Cmplx, /**< a complex multiplication (other roots) */
			} CompType;

		/** @brief the size of each bus on a layer */
		typedef vector<fdim> laySize;

		/** @brief the size of each bus of the FFT */
		typedef vector<laySize> fftSize;
		
		/** @brief the precision required for each bus of the FFT */
		typedef vector<vector<int>> fftPrec;
		/** @brief the error accumulated from the beginning for each bus of 
		    the FFT */
		typedef vector<vector<float>> fftError;

		/**
		 * @brief get the exponant of the twiddle factor of a butterfly
		 * @param[in] layer the layer of the signal
		 * @param[in] signal the line of the signal coming into the butterfly

		 * @return p such as \f$\omega_{2^{\ell}}^{p} \f$ is the twiddle factor
		 * of the butterfly, with \f$\ell\f$ the layer.
		 *
		 * @pre layer < size
		 */
		
		static int getTwiddleExp(const int layer, const int signal);


		/**
		 * @brief say if the signal enters in the non-multiplied(top) part
		 * of its outcoming Butterfly
		 * @param[in] layer the layer of the signal
		 * @param[in] signal the line of the signal coming into the butterfly
		 * @ret true IFF the signal is the input of the top part of its
		 * outcoming Butterfly
		 */
		static bool isTop(const int layer, const int signal);
		
		/**
		 * @brief given one output signal of a butterfly, returns its two input
		 * signals
		 * @param[in] layer the layer of the butterfly's output
		 * @param[in] signal the line of the butterfly's output
		 * @ret the lines of both parents
		 */

		static fdim pred(int layer, int signal);
				
		/**
		 * @brief gets the component in which the signal enter
		 * @param[in] layer the layer of the signal
		 * @param[in] signal the line of the signal coming into the butterfly
		 
		 * @ret the component in which the signal enter
		 */

		static CompType getComponent(int layer, int signal);
		
		/**
		 * @brief computes the error made on each signal in an FFT
		 * @param[in] fft the precision of each element (KCM or Truncate)
		 * @return the size of each signal and the error made on each element 
		 * fft dimensions must be (p, 2**p)
		*/
	
		//static pair<fftSize,fftError> calcDim(fftPrec &fft);

		/**
		 * @brief computes the number of bits of a signal in a radix-4 FFT
		 * @param[in] msbIn the position of the msb of the input
		 * @param[in] lsbIn the position of the lsb of the input 
		 * @param[in] lsbOut precision required, multiplied by sqrt(2)
		 * @param[in] nbLay the number of layers (log 4 pts) of the FFT
		 * @param[in] lay the layer of the signal
		 * @return (a,b) the msb and the lsb of the (signed) signal
		 * This calculation uses the "Projet Ariane" in base 4 formula
		 msbIn is assumed to be 0
		*/

		static fdim sizeSignalAr(int msbIn, int lsbIn,
		                         int lsbOut, int nbLay, int lay);

		/**
		 * @brief computes the number of bits of a signal in a radix-4 FFT
		 * @param[in] msbIn the position of the msb of the input
		 * @param[in] lsbIn the position of the lsb of the input 
		 * @param[in] lsbOut precision required, multiplied by sqrt(2)
		 * @param[in] nbLay the number of layers (log 4 pts) of the FFT
		 * @param[in] lay the layer of the signal
		 * @return (a,b) the msb and the lsb of the (signed) signal
		 * This calculation uses the "Projet Ariane" in base 4 formula, and the
		 So Far So Good principle (exact first layer)
		 msbIn is assumed to be 0
		*/

		static fdim sizeSignalSfsg(int msbIn, int lsbIn, int lsbOut, int nbLay,
		                           int lay);
		
		/**
		 * @brief computes the dimension of each signal in a radix-4 FFT
		 * @param[in] nbLay the numbers of layers (log4 points)
		 * @return the size of each signal and the error made on each element
		 * here we use the "Projet Ariane" formula
		 */
		
		// static laySize calcDim4(int msbIn, int lsbIn, int lsbOut, int nbLay);
	};
}//namespace
#endif
