/**
 * EFM32 Node Demo in ADC sample mode
 */

#include "em_device.h"
#include "em_prs.h"
#include "em_timer.h"
#include "em_adc.h"
#include "em_dma.h"
#include "em_emu.h"
#include "em_cmu.h"
#include "em_chip.h"
#include "em_rtc.h"
#include "em_usart.h"
#include "em_gpio.h"

#include "adc_mode.h"

// Number of ADC samples to reserve space for
#define ADC_SAMPLE_COUNT 100

// Data buffers for ADC samples to be processed
volatile uint16_t adc_data_buffer_1[ADC_SAMPLE_COUNT];
volatile uint16_t adc_data_buffer_2[ADC_SAMPLE_COUNT];

// Currently full data set pointer
volatile uint16_t* adc_valid_data = adc_data_buffer_1;

// DMA complete callback to trigger processing
DMA_CB_TypeDef callback;
void dma_transfer_complete(unsigned int channel, bool primary, void* user);

DMA_DESCRIPTOR_TypeDef dmaControlBlock[128] __attribute__ ((aligned(256)));

/**
 * Run all the below init functions
 */
void adc_mode_init(void)
{
    dma_init();
    adc_init();
    timer_init();

}

/**
 * Handle the RTC timeout by sending data
 */
void rtc_handler(void)
{
    // Stop the timer triggering ADC conversions
    TIMER_Enable(TIMER0, false);

    // Send the current data buffer
    for (uint8_t i = 0; i < ADC_SAMPLE_COUNT * 2; i++)
    {
        USART_SpiTransfer(USART1, adc_valid_data[i]);
    }

    // Restart timer
    TIMER_Enable(TIMER0, true);
}

/**
 * Configure a timer to run at about 80kHz and drive the PRS
 */
void timer_init(void)
{
    // Power the timer up
    CMU_ClockEnable(cmuClock_TIMER0, true);

    const TIMER_Init_TypeDef timerInit =
    {
        .clkSel = timerClkSelHFPerClk,
        .debugRun = false,
        .dmaClrAct = false,
        .enable = false,
        .fallAction = timerInputActionStop,
        .mode = timerModeUp,
        .oneShot = false,
        .prescale = timerPrescale1,
        .quadModeX4 = false,
        .riseAction = timerInputActionReloadStart,
        .sync = false,
    };

    TIMER_Init(TIMER0, &timerInit);

    // Wrap around is about 80kHz
    TIMER_TopSet(TIMER0, CMU_ClockFreqGet(cmuClock_TIMER0)/80000);

    TIMER_Enable(TIMER0, true);
}

/**
 * Fire up the ADC to be PRS triggered, and use DMA
 */
void adc_init(void)
{
    CMU_ClockEnable(cmuClock_ADC0, true);
    CMU_ClockEnable(cmuClock_PRS, true);

    ADC_Init_TypeDef adc_init_data;

    adc_init_data.lpfMode = adcLPFilterBypass;
    adc_init_data.prescale = 0;
    adc_init_data.tailgate = false;
    adc_init_data.timebase = ADC_TimebaseCalc(0);
    adc_init_data.warmUpMode = adcWarmupNormal;

    ADC_Init(ADC0, &adc_init_data);

    ADC_InitSingle_TypeDef adc_single_data;

    adc_single_data.acqTime = adcAcqTime1;
    adc_single_data.diff = false;
    adc_single_data.input = adcSingleInpCh4;
    adc_single_data.leftAdjust = false;
    adc_single_data.prsEnable = true;
    adc_single_data.prsSel = adcPRSSELCh0;
    adc_single_data.reference = adcRefVDD;
    adc_single_data.rep = false;
    adc_single_data.resolution = adcRes12Bit;

    ADC_InitSingle(ADC0, &adc_single_data);

    // Set timer0 overflow as PRS source
    PRS_SourceSignalSet(0, PRS_CH_CTRL_SOURCESEL_TIMER0,
            PRS_CH_CTRL_SIGSEL_TIMER0OF, prsEdgeOff);

    /* Manually set some calibration values */
    ADC0->CAL = (0x7C << _ADC_CAL_SINGLEOFFSET_SHIFT) | (0x1F << _ADC_CAL_SINGLEGAIN_SHIFT);
}

/**
 * Configure DMA to continuously copy ADC results
 */
void dma_init(void)
{
    // Enable clock supply
    CMU_ClockEnable(cmuClock_DMA, true);

    // Activate DMA, disable protection and set control block
    DMA_Init_TypeDef dma_init_data;
    dma_init_data.hprot = 0;
    dma_init_data.controlBlock = dmaControlBlock;
    DMA_Init(&dma_init_data);

    // Configure a callback function
    callback.cbFunc = &dma_transfer_complete;
    callback.userPtr = NULL;

    // Configure a DMA channel
    DMA_CfgChannel_TypeDef dma_channel_data;
    dma_channel_data.cb = &callback;
    dma_channel_data.enableInt = true;
    dma_channel_data.highPri = false;
    dma_channel_data.select = DMAREQ_ADC0_SINGLE;
    DMA_CfgChannel(0, &dma_channel_data);

    // Configure channel descriptor to set behavior
    DMA_CfgDescr_TypeDef dma_descriptor_data;
    dma_descriptor_data.arbRate = dmaArbitrate1;
    dma_descriptor_data.dstInc = dmaDataInc2;
    dma_descriptor_data.hprot = 0;
    dma_descriptor_data.size = dmaDataSize2;
    dma_descriptor_data.srcInc = dmaDataIncNone;

    // We call this twice, once for the primary and once the alternate
    DMA_CfgDescr(0, true, &dma_descriptor_data);
    DMA_CfgDescr(0, false, &dma_descriptor_data);

    // Activate ping-pong stuff
    DMA_ActivatePingPong(0, false, (void *)adc_data_buffer_1,
            (void *)&(ADC0->SINGLEDATA), ADC_SAMPLE_COUNT - 1,
            (void *)adc_data_buffer_2, (void *)&(ADC0->SINGLEDATA),
            ADC_SAMPLE_COUNT - 1);
}

/**
 * Handle DMA transfer completion
 *
 * @param channel  DMA channel where completion happened, not used
 * @param primary  Whether primary or secondary channel completed
 * @param user     Not used.
 */
void dma_transfer_complete(unsigned int channel, bool primary, void* user)
{
    // Re-activate the DMA on the other channel
    DMA_RefreshPingPong(0, primary, false, NULL, NULL, ADC_SAMPLE_COUNT - 1, false);

    // Update the current data buffer
    if (primary == false)
    {
        adc_valid_data = adc_data_buffer_1;
    }
    else
    {
        adc_valid_data = adc_data_buffer_2;
    }
}
