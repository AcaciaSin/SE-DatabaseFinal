#!/bin/bash

rm result/B_tree
./run
xxd result/B_tree > concurrent.hex
rm result/B_tree
./original_run
xxd result/B_tree > serial.hex
