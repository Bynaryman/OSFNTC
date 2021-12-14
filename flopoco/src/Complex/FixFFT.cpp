#include <vector>
#include "FixFFT.hpp"
#include <iostream>

using namespace std;

int uplog2(int number)
{
	int result = 0;
	while(number > 1){
		number >>= 1;
		result++;
	}
	return result;
}

namespace flopoco{
	
	
	int FixFFT::getTwiddleExp(int layer, int signal){
		return signal % (1 << layer);
	}

	bool FixFFT::isTop(int layer, int signal){
		return !(signal & (1<<layer));
	}

	fdim FixFFT::pred(int layer, int signal){
		int pastlay = layer - 1;
		if(isTop(pastlay, signal))
			return make_pair(signal, signal + (1<<pastlay));
		else
			return make_pair(signal - (1<<pastlay), signal);
	}

	FixFFT::CompType FixFFT::getComponent(int layer, int signal){
		int root(1<<(layer+1)); //the main root of unit used for this layer
		int exp(FixFFT::getTwiddleExp(signal, layer));
		//its exponant for the 
		if(isTop(layer, signal))
			return Trunc;
		if(exp && (exp*4) != root)
			return Cmplx;
		return Exact;
	}
	
	//WIP, let's see more theoretic versions first
	//Warning : the use of ints limits the fft size, even if I doubt that 
	// someone will ever need a 4294967296-points FFT
   
	// pair<FixFFT::fftSize, FixFFT::fftError> calcDim(FixFFT::fftPrec &fft){
	// 	int nbLay = fft.size() - 1; // the layers of the FFT
	// 	int nbPts = 1<<nbLay; // the points of the FFT
	// 	if (fft[0].size() != (long unsigned)nbPts)
	// 		throw "not an FFT";
		
	// 	FixFFT::fftSize sizes(nbLay + 1,
	// 	                      FixFFT::laySize(nbPts, fdim (0,0)));

	// 	FixFFT::fftError errors(nbLay, vector<float>(nbPts, 0.0));

	// 	for(int lay=1; lay<=nbLay; lay++){
	// 		for(int pt=0; pt<nbPts; pt++){
	// 			//WIP, not hard, just need to know this sqrt(2) problem
				
	// 		}
	// 	}
		
	// 	return make_pair(sizes,errors);
	// }	

	int arForm(int lsbOut, int nbLay, int lay){
		return lsbOut - 1 - 2*nbLay - uplog2(nbLay) + 2*lay;
	}
	
	fdim FixFFT::sizeSignalAr(int msbIn, int lsbIn, int lsbOut,
	                          int nbLay, int lay){
		if(!lay)
			return make_pair(msbIn, lsbIn);

		return make_pair(msbIn + 2*lay + 1, arForm(lsbOut, nbLay, lay));
	}

	fdim FixFFT::sizeSignalSfsg(int msbIn, int lsbIn, int lsbOut, int nbLay,
	                            int lay){
		switch(lay){
		case 0: return make_pair(msbIn, lsbIn);
		case 1: return make_pair(msbIn + 2, lsbIn);
		default: 
			return make_pair(msbIn + 2*lay + 1,
			                 lsbOut - 1 - 2*nbLay - uplog2(nbLay - 1) + 2*lay);
		}
	}
	
	// FixFFT::laySize FixFFT::calcDim4(int msbIn, int lsbIn, int lsbOut,
	//                                  int nbLay){
	
	// 	FixFFT::laySize size(nbLay+1, fdim (0,0));	
	// 	for(int lay=0; lay <= nbLay; lay++)
	// 		size[lay] = FixFFT::sizeSignalAr(msbIn, lsbIn, lsbOut, nbLay, lay);

	// 	return size;
	// }
}
