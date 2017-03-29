################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/game_compute.c \
../src/platform.c 

LD_SRCS += \
../src/lscript.ld 

OBJS += \
./src/game_compute.o \
./src/platform.o 

C_DEPS += \
./src/game_compute.d \
./src/platform.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MicroBlaze gcc compiler'
	mb-gcc -Wall -O0 -g3 -c -fmessage-length=0 -D __XMK__ -I../../xilkernel_bsp_1/microblaze_1/include -mlittle-endian -mcpu=v9.3 -mxl-soft-mul -Wl,--no-relax -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


