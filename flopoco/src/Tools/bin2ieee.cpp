/*
 * Utility for converting a FP number into its binary representation,
 * for testing etc
 *
 * Author : Florent de Dinechin
 *
 * This file is part of the FloPoCo project developed by the Arenaire
 * team at Ecole Normale Superieure de Lyon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
*/

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <gmpxx.h>
#include <mpfr.h>
#include <cstdlib>

#include"../utils.hpp"
#include"TestBenches/IEEENumber.hpp"
using namespace std;
using namespace flopoco;


static void usage(char *name){
  cerr << endl << "Usage: "<<name<<" wE wF x" << endl ;
  cerr << "  x is a binary string of (wE+wF+1) bits, in IEEE format" << endl ;
  exit (EXIT_FAILURE);
}




int check_strictly_positive(char* s, char* cmd) {
  int n=atoi(s);
  if (n<=0){
    cerr<<"ERROR: got "<<s<<", expected strictly positive number."<<endl;
    usage(cmd);
  }
  return n;
}

int main(int argc, char* argv[] )
{
  if(argc != 4) usage(argv[0]);
  int wE = check_strictly_positive(argv[1], argv[0]);
  int wF = check_strictly_positive(argv[2], argv[0]);
  char* x = argv[3];

  char *p=x;
  int l=0;
	mpz_class z=0;
  while (*p){
    if(*p!='0' && *p!='1') {
      cerr<<"ERROR: expecting a binary string, got "<<argv[3]<<endl;
     usage(argv[0]);
    }
		z=(z<<1)+(*p=='0'?0:1);
    p++; l++;
  }
  if(l != wE+wF+1) {
    cerr<<"ERROR: binary string of size "<< l <<", should be of size "<<wE+wF+1<<endl;
    usage(argv[0]);
  }


	//	cerr << unsignedBinary(z, wE+wF+1) << endl;
	IEEENumber ieeex(wE, wF, z);
	mpfr_t mpfx;
	mpfr_init2(mpfx, wF+1);
	ieeex.getMPFR(mpfx);

	// output on enough bits
  mpfr_out_str (0, // std out
								10, // base
								0, // enough digits so that number may be read back
								mpfx, 
								GMP_RNDN);
  cout << endl;
	
}
