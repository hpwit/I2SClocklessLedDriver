

#include "I2SClocklessLedDriver.h"

#include <soc/rtc.h>


void IRAM_ATTR I2SClocklessLedDriver::interruptHandler(void *arg)
{
    I2SClocklessLedDriver *cont=(I2SClocklessLedDriver *)arg;
  
    if(GET_PERI_REG_BITS(I2S_INT_ST_REG(I2S_DEVICE), I2S_OUT_EOF_INT_ST_V,  I2S_OUT_EOF_INT_ST_S))
    {
        
        if( ((I2SClocklessLedDriver *)arg)->transpose)
        {
            cont->ledToDisplay++;
            if(cont->ledToDisplay<cont->num_led_per_strip)
            {
                cont->loadAndTranspose(cont->leds,cont->num_led_per_strip,cont->num_strips,(uint16_t *)cont->DMABuffersTampon[cont->dmaBufferActive]->buffer,cont->ledToDisplay,cont->__green_map,cont->__red_map,cont->__blue_map,cont->__white_map,cont->nb_components,cont->p_g,cont->p_r,cont->p_b);
                if(cont->ledToDisplay==cont->num_led_per_strip-3)  //here it's not -1 because it takes time top have the change into account and it reread the buufer
                {
                    cont->DMABuffersTampon[cont->dmaBufferActive]->descriptor.qe.stqe_next=&(cont->DMABuffersTampon[3]->descriptor);
                }
                cont->dmaBufferActive = (cont->dmaBufferActive + 1)% 2;
            }
                
        }
        
        
    }
    
    if(GET_PERI_REG_BITS(I2S_INT_ST_REG(I2S_DEVICE), I2S_OUT_TOTAL_EOF_INT_ST_V,  I2S_OUT_TOTAL_EOF_INT_ST_S))
    {
        ((I2SClocklessLedDriver *)arg)->i2sStop();
        if(cont->isWaiting)
        {
            portBASE_TYPE HPTaskAwoken = 0;
            xSemaphoreGiveFromISR(((I2SClocklessLedDriver *)arg)->I2SClocklessLedDriver_sem, &HPTaskAwoken);
            if(HPTaskAwoken == pdTRUE) portYIELD_FROM_ISR();
        }
        
    }
    REG_WRITE(I2S_INT_CLR_REG(0), (REG_READ(I2S_INT_RAW_REG( 0 )) & 0xffffffc0) | 0x3f);       
    
}

