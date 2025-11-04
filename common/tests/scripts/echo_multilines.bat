@echo off
:: echo_multilines.bat
:: Outputs a fixed multi-line content with perfdata, matching test format.
:: Exit code is always 0.

echo OK - load average: 0.00 ^|load1=0.000;5.000;10.000;0; load5=0.000;4.000;8.000;0; load15=0.000;3.000;6.000;0;
echo OK - load1
echo OK - load5
echo OK - load15
exit /b 0