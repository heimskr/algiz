#!/bin/sh
for dir in http net util; do
	find "$dir" -name '*.cpp'
done
ls | grep '\.cpp$'
