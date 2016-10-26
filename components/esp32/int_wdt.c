// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.



#include "sdkconfig.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_types.h>
#include "esp_err.h"
#include "esp_intr.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"

#include "esp_int_wdt.h"

#if CONFIG_INT_WDT


#define WDT_INT_NUM 24


void esp_int_wdt_init() {
    TIMERG1.wdt_wprotect=WDT_WRITE_KEY;
    TIMERG1.wdt_config0.sys_reset_length=7;             //3.2uS
    TIMERG1.wdt_config0.cpu_reset_length=7;             //3.2uS
    TIMERG1.wdt_config0.level_int_en=1;
    TIMERG1.wdt_config0.stg0=1;                         //1st stage timeout: interrupt
    TIMERG1.wdt_config0.stg1=3;                         //2nd stage timeout: reset system
    TIMERG1.wdt_config1.clk_prescale=80*500;            //Prescaler: wdt counts in ticks of 0.5mS
    //The timer configs initially are set to 5 seconds, to make sure the CPU can start up. The tick hook sets
    //it to their actual value.
    TIMERG1.wdt_config2=10000;
    TIMERG1.wdt_config3=10000;
    TIMERG1.wdt_config0.en=1;
    TIMERG1.wdt_feed=1;
    TIMERG1.wdt_wprotect=0;
    TIMERG1.int_clr_timers.wdt=1;
    TIMERG1.int_ena.wdt=1;
    ESP_INTR_DISABLE(WDT_INT_NUM);
    intr_matrix_set(xPortGetCoreID(), ETS_TG1_WDT_LEVEL_INTR_SOURCE, WDT_INT_NUM);
    //We do not register a handler for the interrupt because it is interrupt level 4 which
    //is not servicable from C. Instead, xtensa_vectors.S has a call to the panic handler for
    //this interrupt.
    ESP_INTR_ENABLE(WDT_INT_NUM);
}


//Take care: the tick hook can also be called before esp_int_wdt_init() is called.
#if CONFIG_INT_WDT_CHECK_CPU1
//Not static; the ISR assembly checks this.
bool int_wdt_app_cpu_ticked=false;

void vApplicationTickHook(void) {
    if (xPortGetCoreID()!=0) {
        int_wdt_app_cpu_ticked=true;
    } else {
        //Only feed wdt if app cpu also ticked.
        if (int_wdt_app_cpu_ticked) {
            TIMERG1.wdt_wprotect=WDT_WRITE_KEY;
            TIMERG1.wdt_config2=CONFIG_INT_WDT_TIMEOUT_MS*2;        //Set timeout before interrupt
            TIMERG1.wdt_config3=CONFIG_INT_WDT_TIMEOUT_MS*4;        //Set timeout before reset
            TIMERG1.wdt_feed=1;
            TIMERG1.wdt_wprotect=0;
            int_wdt_app_cpu_ticked=false;
        }
    }
}
#else
void vApplicationTickHook(void) {
    if (xPortGetCoreID()!=0) return;
    TIMERG1.wdt_wprotect=WDT_WRITE_KEY;
    TIMERG1.wdt_config2=CONFIG_INT_WDT_TIMEOUT_MS*2;        //Set timeout before interrupt
    TIMERG1.wdt_config3=CONFIG_INT_WDT_TIMEOUT_MS*4;        //Set timeout before reset
    TIMERG1.wdt_feed=1;
    TIMERG1.wdt_wprotect=0;
}
#endif

#endif