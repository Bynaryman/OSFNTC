from .Posit import *
from .Quire import *
from .LargeAcc import *

def set_posit_env(nbits, es):
    Posit.NBITS = nbits
    Posit.ES = es
    Quire.NBITS = nbits
    Quire.ES = es

def set_large_acc_env(ovf,msb,lsb):
	LargeAcc.OVF = ovf
	LargeAcc.MSB = msb
	LargeAcc.LSB = lsb

