#!/usr/bin/env python
import subprocess
import re
#import warnings
#warnings.filterwarnings("ignore")


import torchvision.models as models_imagenet # these are the default topologies trained with imagenet
import PyTorch_CIFAR10.cifar10_models as models_cifar10 # these modified nn topologies and pretrained

supported_data_sets = ['CIFAR10', 'imagenet']

model_names_imagenet = sorted(name for name in models_imagenet.__dict__
    if name.islower() and not name.startswith("__")
    and callable(models_imagenet.__dict__[name]))

model_names_cifar10 = sorted(name for name in models_cifar10.__dict__
    if name.islower() and not name.startswith("__")
    and callable(models_cifar10.__dict__[name]))

eval_cmd = "LD_LIBRARY_PATH=/opt/OpenBLAS/lib/ OMP_NUM_THREADS=1 VERBOSITY=1 python eval.py --evaluate -p 50 --pretrained --data_set {data_set} -a {model} -b 1 --glimpse"

print("CIFAR10")
for i in model_names_cifar10:
	res = subprocess.check_output(eval_cmd.format(data_set="CIFAR10", model=i), shell=True)
	val = re.search("RET_VAL:.*", res.decode("utf-8"), re.M)
	str_res=val.group(0)[8:].split(",")
	print("{},{},{},{}".format(i,str_res[0],str_res[1],str_res[2]))

print("\nImageNet")
for i in model_names_imagenet:
	res = subprocess.check_output(eval_cmd.format(data_set="IMAGENET", model=i), shell=True)
	val = re.search("RET_VAL:.*", res.decode("utf-8"), re.M)
	str_res=val.group(0)[8:].split(",")
	print("{},{},{},{}".format(i,str_res[0],str_res[1],str_res[2]))
