rm -R AutotestResults/tmp
nbTests=$(grep -c './flopoco' AutotestResults/$1/report)
nbErrors=$(grep -c ERROR AutotestResults/$1/report)
nbVHDL=$(grep -c "VHDL generated"  AutotestResults/$1/report)
nbSuccess=$((nbVHDL-nbErrors))
rateError=$(((nbErrors*100)/nbTests))
rateSuccess=$(((nbSuccess*100)/nbTests))
rateVHDL=$(((nbVHDL*100)/nbTests))
echo "VHDL : $rateVHDL% generated" >> AutotestResults/report
echo "Simulation: $rateError%   failures" >> AutotestResults/report
echo "            $rateSuccess% success" >> AutotestResults/report
echo "See report in AutotestResults/$1 for details" >> AutotestResults/report
echo "VHDL : $rateVHDL% generated"
echo "Simulation success rate:   $rateSuccess% "
echo "See report in AutotestResults/$1 for details"
