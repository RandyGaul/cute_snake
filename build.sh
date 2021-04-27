#!/bin/bash

mkdir -p build_bash
cmake -Bbuild_bash .
cmake --build build_bash
