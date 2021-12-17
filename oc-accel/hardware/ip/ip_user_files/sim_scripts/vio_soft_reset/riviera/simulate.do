onbreak {quit -force}
onerror {quit -force}

asim -t 1ps +access +r +m+vio_soft_reset -L xil_defaultlib -L secureip -O5 xil_defaultlib.vio_soft_reset

do {wave.do}

view wave
view structure

do {vio_soft_reset.udo}

run -all

endsim

quit -force
