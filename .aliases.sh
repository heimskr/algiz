#!/bin/bash

BUILDDIR="${ALGIZ_BUILDDIR:-builddir}"
b() { cd $BUILDDIR && { ninja; cd ..; }; }
alias reconf="CXX='ccache clang++' CC='ccache clang' CXX_LD='mold' meson setup --reconfigure $BUILDDIR ."
alias t="./$BUILDDIR/src/algiz"
bts() { b && ts; }
bt() { b && t; }
