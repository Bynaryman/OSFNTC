#!/usr/bin/env python

import torch
import torchvision.models as models
#from torch.profiler import profile, record_function, ProfilerActivity
model = models.resnet101()
inputs = torch.randn(50, 3, 224, 224)

#with profile(activities=[ProfilerActivity.CPU], record_shapes=True) as prof:
#	with record_function("model_inference"):
#print(prof.key_averages().table(sort_by="cpu_time_total", row_limit=10))
#print("==============")
#prof.export_chrome_trace("trace.json")
#print(prof.key_averages(group_by_input_shape=True).table(sort_by="cpu_time_total", row_limit=50))

model(inputs)
