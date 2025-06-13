#ifndef SOURCE_API_LED_API_H_
#define SOURCE_API_LED_API_H_

/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include "framework_config.h"

/**********************************************************************************************************************
 * Exported definitions and macros
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Exported types
 *********************************************************************************************************************/

/* clang-format off */
typedef enum eLed {
    eLed_First = 0,

    #ifdef USE_ONBOARD_LED
    eLed_OnboardLed,
    #endif

    eLed_Last
} eLed_t;

typedef enum eLedPwm {
    eLedPwm_First = 0,

    #ifdef USE_PULSE_LED
    eLedPwm_PulseLed,
    #endif

    eLedPwm_Last
} eLedPwm_t;
/* clang-format off */

/**********************************************************************************************************************
 * Exported variables
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of exported functions
 *********************************************************************************************************************/

bool LED_API_Init (void);
bool LED_API_TurnOn (const eLed_t led);
bool LED_API_TurnOff (const eLed_t led);
bool LED_API_Toggle (const eLed_t led);
bool LED_API_Blink (const eLed_t led, const uint8_t blink_time, const uint16_t blink_frequency);
bool LED_API_Set_Brightness (const eLedPwm_t led, const uint8_t brightness) ;
bool LED_API_Pulse (const eLedPwm_t led, const uint8_t pulsing_time, const uint16_t pulse_frequency);
bool LED_API_IsCorrectLed (const eLed_t led);
bool LED_API_IsCorrectBlinkTime (const uint8_t blink_time);
bool LED_API_IsCorrectBlinkFrequency (const uint16_t blink_frequency);
bool LED_API_IsCorrectPwmLed (const eLedPwm_t led);
bool LED_API_IsCorrectDutyCycle (const eLedPwm_t led, const uint8_t duty_cycle);
bool LED_API_IsCorrectPulseTime (const uint8_t pulse_time);
bool LED_API_IsCorrectPulseFrequency (const uint16_t pulse_frequency);

#endif /* SOURCE_API_LED_API_H_ */
