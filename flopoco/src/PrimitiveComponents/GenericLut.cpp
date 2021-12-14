// general c++ library for manipulating streams
#include <iostream>
#include <sstream>

// include the header of the Operator
#include "GenericLut.hpp"
#include "Xilinx/Xilinx_Primitive.hpp"

using namespace std;

namespace flopoco {
    GenericLut::GenericLut(Operator* parentOp, Target *target, const std::string &name, const std::map<unsigned int, unsigned int> &pairs, const unsigned int &wIn, const unsigned int &wOut ) : Operator( parentOp,target) {

        setCopyrightString( "Marco Kleinlein" );
        setCombinatorial();
        stringstream tname;
        tname << "GenericLut_" << name;
        srcFileName = "GenericLut";
        setNameWithFreqAndUID( tname.str() );
        wOut_ = wOut;
        wIn_ = wIn;

        if( wOut_ == 0 ) {
            std::map<unsigned int, unsigned int>::const_iterator el = std::max_element( pairs.begin(), pairs.end(),
            []( const std::pair<unsigned int, unsigned int> &p1, const std::pair<unsigned int, unsigned int> &p2 ) {
                return p1.second < p2.second;
            } );
            unsigned int x = el->second;
            unsigned int c = 0;

            while( x > 0 ) {
                c++;
                x >>= 1;
            }

            wOut_ = c;
        }

        if( wIn_ == 0 ) {
            std::map<unsigned int, unsigned int>::const_iterator el = std::max_element( pairs.begin(), pairs.end(),
            []( const std::pair<unsigned int, unsigned int> &p1, const std::pair<unsigned int, unsigned int> &p2 ) {
                return p1.first < p2.first;
            } );
            unsigned int x = el->first;
            unsigned int c = 0;

            while( x > 0 ) {
                c++;
                x >>= 1;
            }

            wIn_ = c;
        }

        for( unsigned int i = 0; i < wIn_; ++i ) {
            addInput( join( "i", i ) );
        }

        for( unsigned int i = 0; i < wOut_; ++i ) {
            addOutput( join( "o", i ) );
        }

        ////////Build Lut////////
        if( target->useTargetOptimizations() ) {
            //build_lut
            throw std::runtime_error( "Target specific optimizations for GenericLut not implemented yet." );
        } else {

            declare( "t_in", wIn_ );

            for( uint i = 0; i < wIn_; ++i ) {

                vhdl << tab << "t_in(" << i << ") <= " << join( "i", i ) << ";" << endl;
            }

            declare( "t_out", wOut_ );
            vhdl << tab << "with t_in select t_out <= " << std::endl;

            for(auto mapIt=pairs.begin(); mapIt!=pairs.end(); ++mapIt)
            {
                unsigned int inputNumber=mapIt->first;
                unsigned int outputNumber=mapIt->second;

                vhdl << tab << tab << "\"";

                //build binary output out of output number
                for(unsigned int i=0; i<this->wOut_; i++)
                {
                    vhdl << ((outputNumber>>(this->wOut_-1-i))&1);
                }

                vhdl << "\" when \"";
                //build binary input out of input number
                for(unsigned int i=0; i<this->wIn_; i++)
                {
                    vhdl << ((inputNumber>>(this->wIn_-1-i))&1);
                }
                vhdl << "\"," << endl;

            }
            //set default case
            vhdl << tab << tab << "\"";
            for(unsigned int i=0; i<wOut_; i++)
            {
                vhdl << "0";
            }
            vhdl << "\" when others;" << endl << endl;

            //build output signals
            for( unsigned int i = 0; i < wOut_; ++i ) {
                vhdl << tab << join( "o", i ) << " <= " << "t_out" << of( i ) << ";" << std::endl;
            }
        }

//        ////////Build Lut using boolean equations//////////
//        for( unsigned int out = 0; out < wOut_; ++out ) {
//            const unsigned int mask = ( 1 << out );
////            cout << endl << "out = " << out << endl;
////            cout << "mask = " << mask << endl << endl;
//            bool_eq eq;
//            for( std::map<unsigned int, unsigned int>::const_iterator it = pairs.begin();
//                 it != pairs.end(); ++it ) {
////                cout << "it->second = " << it->second << endl;
////                cout << "mask = " << mask << endl;
//                if( it->second & mask ) {
//                    bool_eq part;
////                    cout << "it->second & mask (" << it->second << " & " << mask << ")" << endl;
//                    for( unsigned int in = 0; in < wIn_; ++in ) {
////                        cout << "it->first = " << it->first << endl;
////                        cout << "(1 << in) = " << (1 << in) << endl;
//                        if( it->first & ( 1 << in ) ) {
////                            cout << "if" << endl;
//                            part &= bool_eq::in( in );
//                        } else {
////                            cout << "else" << endl;

////                            //////START DEBUG///////
////                            cout << endl << "starting debug of tempEq=";
////                            bool_eq tempEq;
////                            tempEq=bool_eq::in( in );

////                            cout << "in(" << in << ")\n";
////                            vector<bool> temporaryInputVec(this->wIn_);
////                            for(unsigned int x=0; x<(1<<(this->wIn_)); x++)
////                            {
////                                for(unsigned int y=0; y<=in; y++)
////                                {
////                                    temporaryInputVec[y]=(1<<y)&x;
////                                }
////                                cout << tab << "x=";
////                                for(unsigned int y=0; y<temporaryInputVec.size(); y++)
////                                {
////                                    cout << temporaryInputVec[temporaryInputVec.size()-1-y];
////                                }
////                                cout << endl;
////                                cout << tab << tab << "output=" << tempEq.eval(temporaryInputVec) << endl;
////                            }


////                            cout << endl << "starting debug of tempEq=~tempEq" << endl;

////                            bool_eq tempEqInv=~(tempEq);
////                            for(unsigned int x=0; x<(1<<(this->wIn_)); x++)
////                            {
////                                for(unsigned int y=0; y<(1<<(this->wIn_)); y++)
////                                {
////                                    temporaryInputVec[y]=(1<<y)&x;
////                                }
////                                cout << tab << "x=";
////                                for(unsigned int y=0; y<temporaryInputVec.size(); y++)
////                                {
////                                    cout << temporaryInputVec[temporaryInputVec.size()-1-y];
////                                }
////                                cout << endl;
////                                cout << tab << tab << "output=" << tempEqInv.eval(temporaryInputVec) << endl;
////                            }

////                            /////END DEBUG///////

//                            part &= ~bool_eq::in( in ); //part &= ~(bool_eq::in( in ));
//                        }
//                    }
////                    /////START DEBUG///////
////                    cout << "evaluating part" << endl;
////                    vector<bool> tempInputVecPart(this->wIn_);
////                    for(unsigned int i=0; i<(1<<this->wIn_); i++)
////                    {
////                        //create temporary input vector
////                        for(unsigned int j=0; j<this->wIn_; j++)
////                        {
////                            tempInputVecPart[j]=((1<<j)&i);
////                        }
////                        cout << tab << "input: ";
////                        for(unsigned int k=0; k<tempInputVecPart.size(); k++)
////                        {
////                            cout << tempInputVecPart[tempInputVecPart.size()-1-k];
////                        }
////                        cout << "='" << i << "'" << endl;
////                        cout << tab << "output: " << part.eval(tempInputVecPart) << endl;
////                    }
////                    /////END DEBUG///////
////                    cout << "eq |= part" << endl;
//                    eq |= part;
//                }
//            }
////            cout << "push back eq in equations_" << endl << endl;
//            equations_.push_back( eq );
//            vector<bool> tempInputVec(this->wIn_);
////            ///////DEBUG START///////
////            cout << "evaluating eq number " << this->equations_.size()-1 << endl;
////            for(unsigned int i=0; i<(1<<this->wIn_); i++)
////            {
////                //create temporary input vector
////                for(unsigned int j=0; j<this->wIn_; j++)
////                {
////                    tempInputVec[j]=((1<<j)&i);
////                }
////                cout << tab << "input: ";
////                for(unsigned int k=0; k<tempInputVec.size(); k++)
////                {
////                    cout << tempInputVec[tempInputVec.size()-1-k];
////                }
////                cout << "='" << i << "'" << endl;
////                cout << tab << "output: " << eq.eval(tempInputVec) << endl;
////            }
////            ///////DEBUG END///////
//        }

//        build();
    }

    void GenericLut::build() {
        if( getTarget()->useTargetOptimizations() ) {
            //build_lut
            throw std::runtime_error( "Target specific optimizations for GenericLut not implemented yet." );
        } else {
            build_select();
        }
    }

    void GenericLut::build_select() {
        declare( "t_in", wIn_ );

        for( uint i = 0; i < wIn_; ++i ) {

            vhdl << "t_in(" << i << ") <= " << join( "i", i ) << ";" << endl;
        }

        declare( "t_out", wOut_ );
        vhdl << tab << "with t_in select t_out <= " << std::endl;
        const uint max_val = ( 1 << wIn_ );

        for( uint i = 0; i < max_val; ++i ) {
            std::vector<bool> input_vec( wIn_ );
            std::vector<bool> output_vec( wOut_ );

            for( uint j = 0; j < wIn_; ++j ) { // eingangswert
                input_vec[j] = i & ( 1 << j );
            }

            for( uint j = 0; j < wOut_; ++j ) { // ausgangswert
                output_vec[j] = equations_[j].eval( input_vec );
            }

            bool some_set = false;

            for( uint i = 0; i < wOut_; ++i ) {
                if( output_vec[i] ) {
                    some_set = true;
                    break;
                }
            }

            // write
            if( some_set ) {
                vhdl << tab << tab << "\"";

                for( std::vector<bool>::reverse_iterator it = output_vec.rbegin(); it != output_vec.rend(); ++it ) {
                    vhdl << ( *it ? "1" : "0" );
                }

                vhdl << "\" when \"";

                for( std::vector<bool>::reverse_iterator it = input_vec.rbegin(); it != input_vec.rend(); ++it ) {
                    vhdl << ( *it ? "1" : "0" );
                }

                vhdl << "\"," << std::endl;
            }
        }

        vhdl << tab << tab << "\"";

        for( uint i = 0; i < wOut_; ++i ) {
            vhdl << "0";
        }

        vhdl << "\" when others;" << std::endl << std::endl;

        for( unsigned int i = 0; i < wOut_; ++i ) {
            vhdl << tab << join( "o", i ) << " <= " << "t_out" << of( i ) << ";" << std::endl;
        }
    }

    GenericLut::GenericLut(Operator* parentOp, Target *target, const std::string &name, const std::vector<bool_eq> &equations ): Operator( parentOp,target), equations_( equations ) {
        setCopyrightString( "Marco Kleinlein" );
        Xilinx_Primitive::checkTargetCompatibility( target );
        setCombinatorial();
        stringstream tname;
        tname << "GenericLut_" << name;
        srcFileName = "GenericLut";
        setNameWithFreqAndUID( tname.str() );
        wIn_ = 0;

        for( unsigned int i = 0; i < equations_.size(); ++i ) {
            const std::vector<unsigned long> inputs = equations_.at( i ).getInputs();
            std::vector<unsigned long>::const_iterator max = std::max_element( inputs.begin(), inputs.end() );

            if( *max > wIn_ ) {
                wIn_ = *max;
            }
        }

        wOut_ = equations_.size();
        build();
    }

	OperatorPtr GenericLut::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
    std::string entityName;
    std::vector<int> inputValues,outputValues;
    std::map<unsigned int, unsigned int> valueMap;
    int wIn, wOut;
    UserInterface::parseStrictlyPositiveInt(args, "wIn", &wIn);
    UserInterface::parseStrictlyPositiveInt(args, "wOut", &wOut);
    UserInterface::parseString(args, "entityName", &entityName);
    UserInterface::parseColonSeparatedIntList(args, "inputValues", &inputValues);
    UserInterface::parseColonSeparatedIntList(args, "outputValues", &outputValues);
    if(inputValues.size() == outputValues.size()) {
        for(unsigned int i=0; i<inputValues.size(); ++i) {
            valueMap[(unsigned int)inputValues[i]] = (unsigned int)outputValues[i];
        }
    }
		return new GenericLut(parentOp,target,entityName,valueMap,(unsigned int)wIn,(unsigned int)wOut);
	}

	void GenericLut::registerFactory() {
      UserInterface::add("GenericLut", // name
                         "A simple look up table.",
                         "ShiftersLZOCs", // category
                         "", // see also
                         "wIn(int): input word size;\
                        wOut(int): output word size;\
                        entityName(string): unique name for the LUT;\
                        inputValues(string): colon seperated list of (unsigned) ints specifying the inputs for the LUT;\
                        outputValues(string): colon seperated list of (unsigned) ints specifying the corrisponding outputs",
                         "", // no particular extra doc needed
                         GenericLut::parseArguments,
                         nullptr
      );
	}

}//namespace
