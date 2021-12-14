mkdir AutotestResults/$1 > /dev/null
mkdir AutotestResults/tmp > /dev/null 2>&1
echo -n "" >> AutotestResults/$1/report
echo -n "" >> AutotestResults/$1/messages
echo "Testing Operator : $1" >> AutotestResults/report
echo "Testing Operator : $1"
