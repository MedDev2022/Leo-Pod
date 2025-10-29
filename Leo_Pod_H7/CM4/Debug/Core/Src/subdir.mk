################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Core/Src/Client.cpp \
../Core/Src/Fpga.cpp \
../Core/Src/IRay.cpp \
../Core/Src/LRX20A.cpp \
../Core/Src/RPLens.cpp \
../Core/Src/UartDevice.cpp \
../Core/Src/comm.cpp \
../Core/Src/main.cpp \
../Core/Src/uart.cpp 

C_SRCS += \
../Core/Src/main.c \
../Core/Src/resmgr_utility.c \
../Core/Src/stm32h7xx_hal_msp.c \
../Core/Src/stm32h7xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c 

C_DEPS += \
./Core/Src/main.d \
./Core/Src/resmgr_utility.d \
./Core/Src/stm32h7xx_hal_msp.d \
./Core/Src/stm32h7xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d 

OBJS += \
./Core/Src/Client.o \
./Core/Src/Fpga.o \
./Core/Src/IRay.o \
./Core/Src/LRX20A.o \
./Core/Src/RPLens.o \
./Core/Src/UartDevice.o \
./Core/Src/comm.o \
./Core/Src/main.o \
./Core/Src/resmgr_utility.o \
./Core/Src/stm32h7xx_hal_msp.o \
./Core/Src/stm32h7xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/uart.o 

CPP_DEPS += \
./Core/Src/Client.d \
./Core/Src/Fpga.d \
./Core/Src/IRay.d \
./Core/Src/LRX20A.d \
./Core/Src/RPLens.d \
./Core/Src/UartDevice.d \
./Core/Src/comm.d \
./Core/Src/main.d \
./Core/Src/uart.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.cpp Core/Src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++14 -g3 -DDEBUG -DCORE_CM4 -DUSE_HAL_DRIVER -DSTM32H755xx -c -I../Core/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../Drivers/CMSIS/Include -I../../Common/Inc -I../../Utilities/ResourcesManager -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DCORE_CM4 -DUSE_HAL_DRIVER -DSTM32H755xx -c -I../Core/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../Drivers/CMSIS/Include -I../../Common/Inc -I../../Utilities/ResourcesManager -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/Client.cyclo ./Core/Src/Client.d ./Core/Src/Client.o ./Core/Src/Client.su ./Core/Src/Fpga.cyclo ./Core/Src/Fpga.d ./Core/Src/Fpga.o ./Core/Src/Fpga.su ./Core/Src/IRay.cyclo ./Core/Src/IRay.d ./Core/Src/IRay.o ./Core/Src/IRay.su ./Core/Src/LRX20A.cyclo ./Core/Src/LRX20A.d ./Core/Src/LRX20A.o ./Core/Src/LRX20A.su ./Core/Src/RPLens.cyclo ./Core/Src/RPLens.d ./Core/Src/RPLens.o ./Core/Src/RPLens.su ./Core/Src/UartDevice.cyclo ./Core/Src/UartDevice.d ./Core/Src/UartDevice.o ./Core/Src/UartDevice.su ./Core/Src/comm.cyclo ./Core/Src/comm.d ./Core/Src/comm.o ./Core/Src/comm.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/resmgr_utility.cyclo ./Core/Src/resmgr_utility.d ./Core/Src/resmgr_utility.o ./Core/Src/resmgr_utility.su ./Core/Src/stm32h7xx_hal_msp.cyclo ./Core/Src/stm32h7xx_hal_msp.d ./Core/Src/stm32h7xx_hal_msp.o ./Core/Src/stm32h7xx_hal_msp.su ./Core/Src/stm32h7xx_it.cyclo ./Core/Src/stm32h7xx_it.d ./Core/Src/stm32h7xx_it.o ./Core/Src/stm32h7xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/uart.cyclo ./Core/Src/uart.d ./Core/Src/uart.o ./Core/Src/uart.su

.PHONY: clean-Core-2f-Src

