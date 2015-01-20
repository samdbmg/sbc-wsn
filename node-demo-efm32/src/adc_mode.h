/**
 * EFM32 Node Demo in ADC sample mode header
 */
#ifndef ADC_MODE_H_
#define ADC_MODE_H_

// Number of ADC samples to reserve space for
#define ADC_SAMPLE_COUNT 100

// ADC data buffer
extern volatile uint16_t* adc_valid_data;

void adc_mode_init(void);

void timer_init(void);
void adc_init(void);
void dma_init(void);
void dma_transfer_complete(unsigned int channel, bool primary, void* user);

void rtc_handler(void);

#endif /* ADC_MODE_H_ */
