onbreak {quit -f}
onerror {quit -f}

vsim -voptargs="+acc" -t 1ps -L xil_defaultlib -L secureip -lib xil_defaultlib xil_defaultlib.vio_soft_reset

do {wave.do}

view wave
view structure
view signals

do {vio_soft_reset.udo}

run -all

quit -force
