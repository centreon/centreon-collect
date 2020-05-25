<<<<<<< HEAD
#!/bin/sh

echo "ut"

exit 0
=======
#!/bin/bash

echo "ut"

conan install ../..

return_code=$?
if [ $return_code -ne 0 ]
then
	echo "conan failure"
	exit $return_code
fi

./bin/cmake ../..
return_code=$?
if [ $return_code -ne 0 ]
then
	echo "cmake failure"
	exit $return_code
fi

make -j8
return_code=$?
if [ $return_code -ne 0 ]
then
	echo "make failure"
	exit $return_code
fi

./bin/clib-ut --gtest_output="xml:./clib.xml"
return_code=$?
if [ $return_code -ne 0 ]
then
	echo "clib ut failure"
	exit $return_code
fi

./bin/ccc_ut --gtest_output="xml:./connectors.xml"
return_code=$?
if [ $return_code -ne 0 ]
then
	echo "connectors ut failure"
#	exit $return_code
fi

./bin/cce_ut --gtest_output="xml:./engine.xml"
return_code=$?
if [ $return_code -ne 0 ]
then
	echo "engine ut failure"
	exit $return_code
fi

./bin/ccb_ut --gtest_output="xml:./broker.xml"
return_code=$?
if [ $return_code -ne 0 ]
then
	echo "broker ut failure"
	exit $return_code
fi

exit 0


>>>>>>> 73c123c00a38c6b48f219f9389d448d2cb8e05ca
