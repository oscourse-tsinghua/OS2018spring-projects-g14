# OS2018spring-projects-g14

http://os.cs.tsinghua.edu.cn/oscourse/OS2018spring/projects/g14

## Current Progress

We are working on ucore plus for amd64 smp porting.
You can chekout the "amd64-smp" branch to see the newest progress of ucore plus.

## Quick Try

1.  download or clone ucore plus source code
1.  cd ucore
1.  make ARCH=i386 defconfig
1.  make
1.  make sfsimg
1.  uCore_run -d obj

## Makefile

**Cross Compile**

set the environment variables:

export ARCH = ?

you can use archs: i386, arm, amd64, mips, or32, um, nios2

export CROSS_COMPILE = ?

(see Makefile in ./ucore)

**Kconfig**

The top level Kconfig is ./ucore/arch/xxx/Kconfig. You can include other Kconfig

(see ./ucore/arch/arm/Kconfig)

All config option is prefixed with UCONFIG\_

**Makefile**

Supported variables: obj-y, dirs-y

(See ./ucore/kern-ucore/Makefile.build and Makefile.subdir)

**Add a new ARCH**

In arch/xxx, add Makefile.image and include.mk

\*\*\*include.mk: define ARCH_INCLUDES and ARCH_CFLAGD, etc.

\*\*\*Makefile.image: how to generate the executable for your platform.

\*\*\*Kconfig: your top level Kconfig

\*\*\*Makefile: recursively add Makefile in your arch directory.

## More Document

See ucore/doc
