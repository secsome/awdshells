# !/bin/sh

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DSTATIC=True ..
cmake --build . --config=Release -- -j 8
