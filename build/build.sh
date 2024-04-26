#!/bin/bash

# For running tests
ninjaTest()
{
	echo "Running unit tests..."
	ninja test
}

echo "Building..."

cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
conan install .. --profile ../conanprofile.toml --build missing
ninja

while getopts ":test" testing; do
	case $testing in
		test) # run unit tests
			echo "Running unit tests..."
			ninjaTest
			exit;;
		\?) # invalid option
			echo "Error: invalid argument to run unit tests automatically do: ./build.sh -test"
			exit;;
	esac
done

echo "Script finished running!"
echo "You can go into the build folder and run the project by executing: ./network-monitor(.exe)"
