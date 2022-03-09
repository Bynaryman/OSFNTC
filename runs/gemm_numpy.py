#!/usr/bin/env python
import numpy as np

a = np.array([[1, 0], [0, 1]])
b = np.array([[1, 0], [0, 1]])

np.matmul(a, b)

m=16
n=14
k=10

a = np.random.random((m,k))
b = np.random.random((k,n))
#print(a)
#print(b)
print(a[0][0])

C = np.matmul(a, b)

print(C)
