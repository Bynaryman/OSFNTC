#!/usr/bin/env python
import numpy as np

a = np.array([[1, 0], [0, 1]])
b = np.array([[1, 0], [0, 1]])

np.matmul(a, b)

m=32
n=31
k=100

a = np.random.random((m,k))
b = np.random.random((k,n))
print(a)
print(b)
print(a[0][0])

print(np.matmul(a, b))
