#! /bin/bash

# CFLAGS="-fPIC -g -O0 -m64 -D__NS3_COSIM__ -D__KERNEL__ -D__LINSCHED__ -Wall -Wundef -Wstrict-prototypes -Werror-implicit-function-declaration -fno-common  -Wno-pointer-sign -Wno-deprecated"
# CFLAGS="$CFLAGS -I $PWD/src/linsched/model/include"
# CFLAGS="$CFLAGS -I $PWD/src/linsched/model/arch/x86/include"
# CFLAGS="$CFLAGS -include $PWD/src/linsched/model/include/linux/autoconf.h"
# CFLAGS="$CFLAGS -include $PWD/src/linsched/model/linsched/linux_linsched.h"

./waf configure
CXXFLAGS="-Wall -g -O0 -Wno-deprecated"
./waf

