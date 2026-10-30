#!/bin/bash
# -*- coding: utf-8 -*-
#========================================================================
#        author   : masw
#        email    : masw@masw.tech     
#        creattime: 2023年05月09日 火曜日 00時22分51秒
#========================================================================


if [ ! -d build ]; then
	mkdir build
fi

cd build
cmake ../
make 
cd -
cp ./build/iic_test .
