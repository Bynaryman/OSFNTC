cd AutotestResults/tmp
echo "-------------------------------------------" >> ../$1/report
echo "./flopoco $*"
echo "./flopoco $*" >> ../$1/report
echo "./flopoco $*" >> ../$1/messages
../../flopoco $* > temp 2>&1
if grep 'gtkwave' temp > /dev/null; then
	echo -n 'VHDL generated | ' >> ../$1/report
	ghdl=$(grep 'ghdl -a' temp)
	if $ghdl >> ../$1/messages 2>&1; then	
		echo -n 'GHDL -a SUCCESS | ' >> ../$1/report
	else
		echo -n 'GHDL -a ERROR | ' >> ../$1/report
		echo  'GHDL -a ERROR'
	fi
	ghdl=$(grep 'ghdl -e' temp)
	if $ghdl >> ../$1/messages 2>&1; then
		echo -n 'GHDL -e SUCCESS | ' >> ../$1/report
	else
		echo -n 'GHDL -e ERROR | ' >> ../$1/report
		echo 'GHDL -e ERROR'
	fi

	ghdl=$(grep 'ghdl -r' temp)
	$ghdl > ghdl 2>&1
	nbError=$(grep -c error ghdl)
	normalNbError=1
	successNoError=false
	successError=true
	if grep 'error(s) encoutered' ghdl > /dev/null; then
		successError=false
	fi
	if grep "simulation stopped by --stop-time" ghdl > /dev/null; then
		successNoError=true
	fi
	if $successError && $successNoError; then
		echo 'GHDL -r SUCCESS' >> ../$1/report
	else
		if ! $successError; then
			if [ $nbError -eq $normalNbError ]; then
				echo 'GHDL -r SUCCESS' >> ../$1/report
			else
				echo 'GHDL -r ERROR' >> ../$1/report
				echo 'GHDL -r ERROR'
				cat ghdl >> ../$1/messages
			fi
		else
			echo 'GHDL -r ERROR' >> ../$1/report
			echo 'GHDL -r ERROR'
			cat ghdl >> ../$1/messages
		fi
	fi
else
	echo 'VHDL not generated' >> ../$1/report
	echo 'VHDL not generated'
	cat temp >> ../$1/messages
fi
