#!/usr/bin/env python

import torch
import torchvision.models as models
import random
#from torch.profiler import profile, record_function, ProfilerActivity
random.seed(0)
torch.random.manual_seed(2)
model = models.vgg16()
inputs = torch.randn(1, 3, 32, 32)
print(inputs)

#with profile(activities=[ProfilerActivity.CPU], record_shapes=True) as prof:
#	with record_function("model_inference"):
#print(prof.key_averages().table(sort_by="cpu_time_total", row_limit=10))
#print("==============")
#prof.export_chrome_trace("trace.json")
#print(prof.key_averages(group_by_input_shape=True).table(sort_by="cpu_time_total", row_limit=50))

print(model(inputs))
