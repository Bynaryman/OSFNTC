from decimal import Decimal
from copy import *
from FixedPoint import *

class LargeAcc(object):
    def __init__(self, number = 0, ovf=None, msb=None, lsb=None):
        if type(LargeAcc.OVF) is not int or type(LargeAcc.MSB) is not int or type(LargeAcc.LSB) is not int:
            raise Exception("Set large acc envrionemnt first using set_large_acc_env(ovf,msb,lsb)")
        else:
            self.ovf = LargeAcc.OVF
            self.msb = LargeAcc.MSB
            self.lsb = LargeAcc.LSB

        self.fraction_bits = -self.lsb
        self.integer_bits = self.msb+self.ovf

        if type(number) == Posit:
            #print(self.msb, -self.lsb)
            self.family = FXfamily(n_bits = self.fraction_bits, n_intbits = self.integer_bits)
            try:
                self.q = FXnum(val = float(number), family= self.family)
                #print(self.q)
            except:
                print("something happened 1")
            #self.nbits = number.nbits
            #self.es = number.es
            #self.family = FXfamily(n_bits = self.fraction_bits, n_intbits = self.integer_bits)
            #if number.number == 0:
            #    self.q = FXnum(0, family=self.family)
            #elif number.number == number.inf:
            #    raise Exception("Cannot convert to fixed point")

            #sign, regime, exponent, fraction = number.decode()

            #try:
            #    f = FXnum(fraction, family=self.family)
            #    n = countBits(fraction) - 1
            #    self.q = ((-1)**sign * FXnum(2, family=self.family)**Decimal(2**self.es * regime + exponent - n) * FXnum(f, family = self.family))
            #except:
            #    self.q = FXnum(val=0, family=self.family)
        else:
            self.family = FXfamily(n_bits = self.fraction_bits, n_intbits = self.integer_bits)
			#(Fxnum(2 , family=self.family)**Decimal(float_unbiased_exponent))*FXnum
            #self.q = FXnum(val = number, family= self.family)
            #print("number before trick", number)
            self.q = FXnum(1, family= self.family) * Decimal.from_float(number)
            #print("number after", self.q)
            #except as e:
        # elif type(number) == float:
        #     self.family = FXfamily(n_bits = self.fraction_bits, n_intbits = self.integer_bits)
        #     self.q = FXnum(val = number, family= self.family)

    def set_q(self, q):
        self.q = q

    def set_family(self, family):
        self.family = family

    def get_q(self):
        return self.q

    def get_family(self):
        return self.family

    # set largeacc to 0
    def clear(self):
        self.q = FXnum(val = 0, family = self.family)

    def add_posit_product(self, p1, p2):
        if type(p1) == Posit and type(p2) == Posit:
            self.q += LargeAcc(p1) * LargeAcc(p2)
        else:
            raise Exception("Arguments must be posit")

    def sub_posit_product(self, p1, p2):
        if type(p1) == Posit and type(p2) == Posit:
            self.q -= LargeAcc(p1) * LargeAcc(p2)
        else:
            raise Exception("Arguments must be posit")

    def set_int(self, n):
        if type(n) == int:
            self.q = FXnum(val = n, family = self.family)
        else:
            raise "Not int"

    def __add__(self, other):
        ret = deepcopy(self)
        ret.q += other.q
        #print("largeACC int acc", ret.q)
        return ret

    def __sub__(self, other):
        ret = deepcopy(self)
        ret.q -= other.q
        return ret

    def __mul__(self, other):
        ret = deepcopy(self)
        ret.q *= other.q
        return ret

    def __pow__(self, other):
        ret = deepcopy(self)
        ret.q = ret.q**other.q
        return ret

    def __truediv__(self, other):
        ret = deepcopy(self)
        ret.q /= other.q
        return ret

    def __float__(self):
        return float(Decimal(self.q.scaledval)/Decimal(self.q.family.scale))
        #return self.q.__float__()

    def __str__(self):
        return self.q.__str__()

	# Arithmetic comparison tests:
    def __eq__(self, other):
        """Equality test"""
        other = self._CastOrFail_(other)
        return self.scaledval == other.scaledval and self.family == other.family

    def __ne__(self, other):
        """Inequality test"""
        other = self._CastOrFail_(other)
        return self.scaledval != other.scaledval

    def __ge__(self, other):
        """Greater-or-equal test"""
        other = self.q._CastOrFail_(other)
        return self.q.scaledval >= other.q.scaledval

    def reduce2PI(self):
        sign = -1 if self.q.scaledval < 0 else 1
        self.q.scaledval = abs(self.q.scaledval)
        y = copy(self.q)
        t = y / (2 * self.family.pi)
        t.scaledval &= onesComplement((1 << self.fraction_bits) - 1, self.integer_bits + self.fraction_bits)
        self.q = (y - t * (2 * self.family.pi))
        self.q = sign * self.q

from .Posit import *
from .BitUtils import *
