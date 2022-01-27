#!/usr/bin/env python
import numpy as np

a = np.array([[1, 0], [0, 1]])
b = np.array([[1, 0], [0, 1]])

np.matmul(a, b)

a = np.random.random((10000,10000))
b = np.random.random((10000,10000))

print(np.matmul(a, b))
