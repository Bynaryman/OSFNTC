#!/usr/bin/env python
import numpy as np
import sys
import struct
np.set_printoptions(threshold=sys.maxsize)
def double_to_hex(f):
    return hex(struct.unpack('<Q', struct.pack('<d', f))[0])

#a = np.array([[1, 0], [0, 1]])
#b = np.array([[1, 0], [0, 1]])
#
#np.matmul(a, b)

m=123
n=57
k=30

np.random.seed(123)

a = np.random.random((m,k)).astype(np.float32)
b = np.random.random((k,n)).astype(np.float32)
a = 150*a
b = 50*b
print(a)
#print(b)
#print(a[0][0])

#print(double_to_hex(b[0][0]))
#print(double_to_hex(b[1][0]))
#print(double_to_hex(b[0][1]))
#print(double_to_hex(b[k-1][n-1]))

C = np.matmul(a, b, dtype=np.float32)
print(C)

#a = np.identity(64)
#b = np.ones((64,28))
#
#C = np.matmul(a, b)
#print(8)
