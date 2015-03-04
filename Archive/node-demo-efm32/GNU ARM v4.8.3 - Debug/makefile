################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/subdir.mk
-include emlib/subdir.mk
-include CMSIS/efm32zg/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: node-demo-efm32.axf

# Tool invocations
node-demo-efm32.axf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GNU ARM C Linker'
	arm-none-eabi-gcc -g -gdwarf-2 -mcpu=cortex-m0plus -mthumb -T "node-demo-efm32.ld" -Xlinker --gc-sections -Xlinker -Map="node-demo-efm32.map" --specs=nano.specs -o node-demo-efm32.axf "./src/main.o" "./src/pin_mode.o" "./src/pinchange_detect_algorithm.o" "./emlib/em_adc.o" "./emlib/em_assert.o" "./emlib/em_cmu.o" "./emlib/em_dma.o" "./emlib/em_emu.o" "./emlib/em_gpio.o" "./emlib/em_int.o" "./emlib/em_pcnt.o" "./emlib/em_prs.o" "./emlib/em_rtc.o" "./emlib/em_system.o" "./emlib/em_timer.o" "./emlib/em_usart.o" "./CMSIS/efm32zg/startup_gcc_efm32zg.o" "./CMSIS/efm32zg/system_efm32zg.o" -Wl,--start-group -lgcc -lc -lnosys -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

	@echo 'Building hex file: node-demo-efm32.hex'
	arm-none-eabi-objcopy -O ihex "node-demo-efm32.axf" "node-demo-efm32.hex"
	@echo ' '

	@echo 'Building bin file: node-demo-efm32.bin'
	arm-none-eabi-objcopy -O binary "node-demo-efm32.axf" "node-demo-efm32.bin"
	@echo ' '

	@echo 'Running size tool'
	arm-none-eabi-size "node-demo-efm32.axf"
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) node-demo-efm32.axf
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
