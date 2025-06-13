#ifndef SOURCE_APP_LED_APP_H_
#define SOURCE_APP_LED_APP_H_
/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include "led_api.h"
#include "framework_config.h"

/**********************************************************************************************************************
 * Exported definitions and macros
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Exported types
 *********************************************************************************************************************/

/* clang-format off */
typedef enum eLedTask {
    eLedTask_First = 0,
    
    #ifdef USE_LED
    eLedTask_Set,
    eLedTask_Reset,
    eLedTask_Toggle,
    eLedTask_Blink,
    #endif

    #ifdef USE_PWM_LED
    eLedTask_Set_Brightness,
    eLedTask_Pulse,
    #endif
    
    eLedTask_Last
} eLedTask_t;

typedef struct sLedCommandDesc {
    eLedTask_t task;
    void *data;
} sLedCommandDesc_t;

typedef struct sLedCommon {
    eLed_t led;
} sLedCommon_t;

typedef struct sLedBlink {
    eLed_t led;
    uint8_t blink_time;
    uint16_t blink_frequency;
} sLedBlink_t;

typedef struct sLedSetBrightness {
    eLedPwm_t led;
    uint8_t duty_cycle;
} sLedSetBrightness_t;

typedef struct sLedPulse {
    eLedPwm_t led;
    uint8_t pulse_time;
    uint16_t pulse_frequency;
} sLedPulse_t;
/* clang-format on */

/**********************************************************************************************************************
 * Exported variables
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of exported functions
 *********************************************************************************************************************/

bool LED_APP_Init (void);
bool LED_APP_Add_Task (sLedCommandDesc_t *task_to_message_queue);

#endif /* SOURCE_APP_LED_APP_H_ */
