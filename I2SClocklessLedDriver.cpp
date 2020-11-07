

#include "I2SClocklessLedDriver.h"

#include <soc/rtc.h>


 void IRAM_ATTR I2SClocklessLedDriver::interruptHandler(void *arg)
    {
        I2SClocklessLedDriver *cont=(I2SClocklessLedDriver *)arg;
       
        uint8_t d= GET_PERI_REG_BITS(I2S_INT_ST_REG(I2S_DEVICE), I2S_OUT_TOTAL_EOF_INT_ST_V,  I2S_OUT_TOTAL_EOF_INT_ST_S);
        uint8_t d2= GET_PERI_REG_BITS(I2S_INT_ST_REG(I2S_DEVICE), I2S_OUT_EOF_INT_ST_V,  I2S_OUT_EOF_INT_ST_S);
        REG_WRITE(I2S_INT_CLR_REG(0), (REG_READ(I2S_INT_RAW_REG( 0 )) & 0xffffffc0) | 0x3f);
        
              
            if(d2)
            {
                //((I2SClocklessLedDriver *)arg)->eof++;
                // cont->rr[cont->eof]=255;
                if( ((I2SClocklessLedDriver *)arg)->transpose)
                {
                    cont->ledToDisplay++;
                if(cont->ledToDisplay<cont->num_led_per_strip)
                    {
                                             //cont->ledbuff[cont->ledToDisplay]=cont->dmaBufferActive;
                    cont->loadAndTranspose(cont->leds,cont->num_led_per_strip,cont->num_strips,(uint16_t *)cont->DMABuffersTampon[cont->dmaBufferActive]->buffer,cont->ledToDisplay,cont->__green_map,cont->__red_map,cont->__blue_map,cont->__white_map,cont->nb_components,cont->p_g,cont->p_r,cont->p_b);
                    
                    
                    
                    

                        //cont->rr[cont->eof]=cont->dmaBufferActive;
                            if(cont->ledToDisplay==cont->num_led_per_strip-3)  //here it's not -1 because it takes trime top have the change into account and it reread the buufer
                            {
                                 cont->DMABuffersTampon[cont->dmaBufferActive]->descriptor.qe.stqe_next=&(cont->DMABuffersTampon[3]->descriptor);
                               // cont->end=cont->dmaBufferActive;
                            }
                            
                             cont->dmaBufferActive = (cont->dmaBufferActive + 1)% 2;
                    }
                    

                }
                            

            }

     if(d)
              {
                  //((I2SClocklessLedDriver *)arg)->total++;
                   // ((I2SClocklessLedDriver *)arg)->time1=ESP.getCycleCount()-((I2SClocklessLedDriver *)arg)->time1;
                 ((I2SClocklessLedDriver *)arg)->i2sStop();
                    portBASE_TYPE HPTaskAwoken = 0;
                xSemaphoreGiveFromISR(((I2SClocklessLedDriver *)arg)->I2SClocklessLedDriver_sem, &HPTaskAwoken);
                if(HPTaskAwoken == pdTRUE) portYIELD_FROM_ISR()

              }

    }

