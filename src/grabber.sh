#!/bin/sh
for dir in http net util threading; do
	find "$dir" -name '*.cpp'
done
ls | grep '\.cpp$'
