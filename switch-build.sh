#!/bin/bash

if [ "$1" = "-h" ] ; then
	echo "Switch the host to compile on: host vs podman"
	if [ -d build-host ] ; then
		echo "The current configuration compiles on podman."
	else
		echo "The current configuration compiles on the host."
	fi
	exit 0
fi

if [ -d build-host ] ; then
	echo "The current configuration compiles on podman."
	echo "Switching to host"
	mv build build-podman
	mv build-host build
	exit 0
fi

if [ -d build-podman ] ; then
	echo "The current configuration compiles on the host."
	echo "Switching to podman"
	mv build build-host
	mv build-podman build
	exit 0
fi
