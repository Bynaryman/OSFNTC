/*
This file is for debug purpose only
*/
#include <iostream>
#include "FixFFT.hpp"

using namespace flopoco;
using namespace std;

ostream& operator<<(ostream& flux, pair<int,int> p){
	return flux << '(' << get<0>(p) << ',' << get<1>(p) << ')';
}

int main(int argc, char** argv){
	cout << argc << endl;
	if(argc < 5){
		for(int i=1; i<18; i++)
			cout << uplog2(i) << " ";
		return 1;
	}
	int msbIn = stoi(argv[1]);
	int lsbIn = stoi(argv[2]);
	int lsbOut = stoi(argv[3]);
	int nbLay = stoi(argv[4]);
	int nbPts = 1<<(2*nbLay);
	
	
	for(int lay=0; lay <= nbLay; lay++){
		cout << FixFFT::sizeSignalAr(msbIn, lsbIn, lsbOut, nbLay, lay) << " ";
	}
	cout << endl;
	
	for(int lay=0; lay <= nbLay; lay++){
		cout << FixFFT::sizeSignalSfsg(msbIn, lsbIn, lsbOut, nbLay, lay) << " ";
	}
	cout << endl;

	return 0;
}
