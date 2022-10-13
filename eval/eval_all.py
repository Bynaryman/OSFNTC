#!/usr/bin/env python
import os

import torchvision.models as models_imagenet # these are the default topologies trained with imagenet
import PyTorch_CIFAR10.cifar10_models as models_cifar10 # these modified nn topologies and pretrained

GLIMPSE_IMG_NUMBER = 150

supported_data_sets = ['CIFAR10', 'imagenet']

model_names_imagenet = sorted(name for name in models_imagenet.__dict__
    if name.islower() and not name.startswith("__")
    and callable(models_imagenet.__dict__[name]))

model_names_cifar10 = sorted(name for name in models_cifar10.__dict__
    if name.islower() and not name.startswith("__")
    and callable(models_cifar10.__dict__[name]))

eval_cmd = "LD_LIBRARY_PATH=/opt/OpenBLAS/lib/ OMP_NUM_THREADS=1 VERBOSITY=1 python eval.py --evaluate -p 150 --pretrained --data_set {data_set} -a {model} -b 1 --glimpse"

for i in model_names_imagenet:
	os.system(eval_cmd.format(data_set="imagenet", model=i))

for i in model_names_cifar10:
	os.system(eval_cmd.format(data_set="CIFAR10", model=i))
