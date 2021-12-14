#         FPMult wE wF_in wF_out

flopoco FPMult 8 23 23
flopoco FPMult 8 23 52
flopoco FPMult 11 52 23

flopoco -target=StratixII FPMult 8 23 23
flopoco -target=StratixII FPMult 8 23 52
flopoco -target=StratixII FPMult 11 52 23
