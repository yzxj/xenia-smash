######################################################################
# Copyright (c) 2011 Xilinx, Inc.  All rights reserved. 
# 
# Xilinx, Inc. 
# XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A 
# COURTESY TO YOU.  BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS 
# ONE POSSIBLE   IMPLEMENTATION OF THIS FEATURE, APPLICATION OR 
# STANDARD, XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION 
# IS FREE FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE 
# FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION. 
# XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO 
# THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO 
# ANY WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE 
# FROM CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY 
# AND FITNESS FOR A PARTICULAR PURPOSE. 
######################################################################

include config.make

AS=armasm
COMPILER=armcc
ARCHIVER=armar
CP=cp
COMPILER_FLAGS=
EXTRA_COMPILER_FLAGS=

LIB=libxil.a

CC_FLAGS = $(COMPILER_FLAGS)
ECC_FLAGS = $(EXTRA_COMPILER_FLAGS)

ECC_FLAGS	+= --cpu=Cortex-A9 --fpu=VFPv3_FP16 --vectorize

RELEASEDIR=../../../lib
INCLUDEDIR=../../../include

INCLUDES=-I./. -I${INCLUDEDIR}

INCLUDEFILES=*.h

COBJS	=	$(addsuffix .o, $(basename $(wildcard *.c)))
AOBJS	=	asm_vectors.o
AOBJS	+=	translation_table.o
OBJS    = $(COBJS) $(AOBJS) boot_post.o

ASOURCES = asm_vectors.s
ASOURCES += translation_table.s

# Replace boot.o with boot_post.o
OBJECTS = $(OBJS)

libs: boot_libs banner $(LIBS)

boot_libs:
ifeq ($(findstring boot.S,$(wildcard *.S)),boot.S) 
	${COMPILER} $(INCLUDES) -E -o boot_post.s boot.S
endif
	${AS} --cpu=Cortex-A9 --fpu=VFPv3_FP16 -o boot_post.o boot_post.s
	rm -f boot.S 

$(AOBJS): $(ASOURCES)
	${AS} --cpu=Cortex-A9 --fpu=VFPv3_FP16 -o $@ $<

%.o: %.c
	${COMPILER} -c $(CC_FLAGS) $(ECC_FLAGS) $(INCLUDES) -o $@ $<

banner:
	echo "Compiling standalone"

standalone_libs: ${OBJECTS}
	$(ARCHIVER) -r ${RELEASEDIR}/${LIB} ${OBJECTS}

.PHONY: include
include: standalone_includes

standalone_includes:
	${CP} ${INCLUDEFILES} ${INCLUDEDIR}

clean:
	rm -rf ${OBJECTS}
