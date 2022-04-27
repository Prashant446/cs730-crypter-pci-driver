#! /bin/bash

ROOT_DIR="~/CS730-PA-library-master"
TEST_DIR="./eval-test"

cd $ROOT_DIR
make clean && make
cd $TEST_DIR
make clean && make
