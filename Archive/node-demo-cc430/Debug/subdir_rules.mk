################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
main.obj: ../main.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: MSP430 Compiler'
	"C:/Apps/TI/ccsv6/tools/compiler/ti-cgt-msp430_4.4.2/bin/cl430" -vmspx --abi=eabi --use_hw_mpy=F5 --include_path="C:/Apps/TI/ccsv6/ccs_base/msp430/include" --include_path="C:/Apps/TI/ccsv6/tools/compiler/ti-cgt-msp430_4.4.2/include" --include_path="C:/Users/Sam/Documents/Code/sbc-wsn/node-demo-cc430/driverlib/MSP430F5xx_6xx" --advice:power_severity=remark --advice:power="1,2,3,4,5,6,7,8,9,10,11,12,13,14" -g --define=DEPRECATED --define=__CC430F5137__ --diag_warning=225 --display_error_number --diag_wrap=off --silicon_errata=CPU18 --silicon_errata=CPU21 --silicon_errata=CPU22 --silicon_errata=CPU23 --silicon_errata=CPU40 --printf_support=minimal --preproc_with_compile --preproc_dependency="main.pp" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '

pin_mode.obj: ../pin_mode.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: MSP430 Compiler'
	"C:/Apps/TI/ccsv6/tools/compiler/ti-cgt-msp430_4.4.2/bin/cl430" -vmspx --abi=eabi --use_hw_mpy=F5 --include_path="C:/Apps/TI/ccsv6/ccs_base/msp430/include" --include_path="C:/Apps/TI/ccsv6/tools/compiler/ti-cgt-msp430_4.4.2/include" --include_path="C:/Users/Sam/Documents/Code/sbc-wsn/node-demo-cc430/driverlib/MSP430F5xx_6xx" --advice:power_severity=remark --advice:power="1,2,3,4,5,6,7,8,9,10,11,12,13,14" -g --define=DEPRECATED --define=__CC430F5137__ --diag_warning=225 --display_error_number --diag_wrap=off --silicon_errata=CPU18 --silicon_errata=CPU21 --silicon_errata=CPU22 --silicon_errata=CPU23 --silicon_errata=CPU40 --printf_support=minimal --preproc_with_compile --preproc_dependency="pin_mode.pp" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '

pinchange_detect_algorithm.obj: ../pinchange_detect_algorithm.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: MSP430 Compiler'
	"C:/Apps/TI/ccsv6/tools/compiler/ti-cgt-msp430_4.4.2/bin/cl430" -vmspx --abi=eabi --use_hw_mpy=F5 --include_path="C:/Apps/TI/ccsv6/ccs_base/msp430/include" --include_path="C:/Apps/TI/ccsv6/tools/compiler/ti-cgt-msp430_4.4.2/include" --include_path="C:/Users/Sam/Documents/Code/sbc-wsn/node-demo-cc430/driverlib/MSP430F5xx_6xx" --advice:power_severity=remark --advice:power="1,2,3,4,5,6,7,8,9,10,11,12,13,14" -g --define=DEPRECATED --define=__CC430F5137__ --diag_warning=225 --display_error_number --diag_wrap=off --silicon_errata=CPU18 --silicon_errata=CPU21 --silicon_errata=CPU22 --silicon_errata=CPU23 --silicon_errata=CPU40 --printf_support=minimal --preproc_with_compile --preproc_dependency="pinchange_detect_algorithm.pp" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '


