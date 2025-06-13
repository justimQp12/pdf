/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include "led_api.h"

#if defined(USE_LED) || defined(USE_PWM_LED)

#include "cmsis_os2.h"
#include "gpio_driver.h"
#include "pwm_driver.h"
#include "timer_driver.h"

/**********************************************************************************************************************
 * Private definitions and macros
 *********************************************************************************************************************/

#define BLINK_MUTEX_TIMEOUT 0U
#define PULSE_MUTEX_TIMEOUT 0U

#define PULSE_TIMER_FREQUENCY 1

/**********************************************************************************************************************
 * Private typedef
 *********************************************************************************************************************/

typedef struct sLedControlDesc {
    eGpioPin_t led_pin;
    bool is_inverted;
    osTimerAttr_t blink_timer_attributes;
    osMutexAttr_t blink_mutex_attributes;
} sLedControlDesc_t;

typedef struct sLedPwmControlDesc {
    ePwmDevice_t pwm_device;
    osTimerAttr_t pulse_timer_attributes;
    osMutexAttr_t pulse_mutex_attributes;
} sLedPwmControlDesc_t;

typedef struct sLedBlinkDesc {
    eLed_t led;
    osTimerId_t blink_timer;
    osMutexId_t blink_mutex;
    bool is_running;
    void (*timer_callback) (void *arg);
    uint16_t total_blinks;
    uint16_t blink_count;
} sLedBlinkDesc_t; 

typedef struct sLedPulseDesc {
    eLedPwm_t led;
    osTimerId_t pulse_timer;
    osMutexId_t pulse_mutex;
    bool is_running;
    void (*timer_callback) (void *arg);
    bool count_dir_up;
    uint16_t timer_resolution;
    uint16_t duty_cycle_change;
    uint16_t total_pulses;
    uint16_t total_changes_per_pulse;
    uint16_t current_duty_cycle;
    uint16_t pulse_count;
    uint16_t change_count;
} sLedPulseDesc_t;

/**********************************************************************************************************************
 * Private constants
 *********************************************************************************************************************/

/* clang-format off */
#ifdef USE_LED
const static sLedControlDesc_t g_basic_led_control_static_lut[eLed_Last] = {
    #ifdef USE_ONBOARD_LED
    [eLed_OnboardLed] = {
        .led_pin = eGpioPin_OnboardLed,
        .is_inverted = USE_ONBOARD_LED_INVERTED,
        .blink_timer_attributes = {.name = "LED_API_Onboard_LED_Timer", .attr_bits = 0, .cb_mem = NULL, .cb_size = 0},
        .blink_mutex_attributes = {.name = "LED_API_Onboard_LED_Mutex", .attr_bits = osMutexRecursive | osMutexPrioInherit, .cb_mem = NULL, .cb_size = 0U},
    }
    #endif
};
#endif

#ifdef USE_PWM_LED
const static sLedPwmControlDesc_t g_pwm_led_control_static_lut[eLedPwm_Last] = {
    #ifdef USE_PULSE_LED
    [eLedPwm_PulseLed] = {
        .pwm_device = ePwmDevice_PulseLed,
        .pulse_timer_attributes = {.name = "LED_API_Pulse_LED_Timer", .attr_bits = 0, .cb_mem = NULL, .cb_size = 0},
        .pulse_mutex_attributes = {.name = "LED_API_Pulse_LED_Mutex", .attr_bits = osMutexRecursive | osMutexPrioInherit, .cb_mem = NULL, .cb_size = 0U},
    }
    #endif
};
#endif
/* clang-format on */

/**********************************************************************************************************************
 * Prototypes of private functions
 *********************************************************************************************************************/

#ifdef USE_LED
static void LED_API_Blink_Timer_Callback (void *arg);
#endif

#ifdef USE_PWM_LED
static void LED_API_Pulse_timer_Callback (void *arg);
#endif

/**********************************************************************************************************************
 * Private variables
 *********************************************************************************************************************/

static bool g_is_led_initialized = false;
static bool g_is_pwm_initialized = false;

osTimerId_t g_blink_timer = NULL;
uint16_t g_blink_count = 0;

/* clang-format off */
#ifdef USE_LED
static sLedBlinkDesc_t g_led_blink_lut[eLed_Last] = {
    #ifdef USE_ONBOARD_LED
    [eLed_OnboardLed] = {
        .led = eLed_OnboardLed,
        .blink_timer = NULL,
        .blink_mutex = NULL,
        .is_running = false,
        .timer_callback = LED_API_Blink_Timer_Callback,
        .total_blinks = 0,
        .blink_count = 0,
    },
    #endif
};
#endif

#ifdef USE_PWM_LED
static sLedPulseDesc_t g_led_pulse_lut[eLedPwm_Last] = {
    #ifdef USE_PULSE_LED
    [eLedPwm_PulseLed] = {
        .led = eLedPwm_PulseLed,
        .pulse_timer = NULL,
        .pulse_mutex = NULL,
        .is_running = false,
        .timer_callback = LED_API_Pulse_timer_Callback,
        .count_dir_up = true,
        .timer_resolution = 0,
        .duty_cycle_change = 0,
        .total_pulses = 0,
        .total_changes_per_pulse = 0,
        .current_duty_cycle = 0,
        .pulse_count = 0,
        .change_count = 0
    }
    #endif
};
#endif
/* clang-format on */

/**********************************************************************************************************************
 * Exported variables and references
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Definitions of private functions
 *********************************************************************************************************************/

#ifdef USE_LED
static void LED_API_Blink_Timer_Callback (void *arg) {
    sLedBlinkDesc_t *led_blink_desc = (sLedBlinkDesc_t*) arg;

    if (!led_blink_desc->is_running) {
        if (osMutexAcquire(led_blink_desc->blink_mutex, BLINK_MUTEX_TIMEOUT) != osOK) {
            return;
        }
    }

    led_blink_desc->is_running = true;

    osMutexRelease(led_blink_desc->blink_mutex);

    LED_API_Toggle(led_blink_desc->led);

    led_blink_desc->blink_count++;

    if (led_blink_desc->blink_count >= led_blink_desc->total_blinks){
        osTimerStop(led_blink_desc->blink_timer);
        
        LED_API_TurnOff(led_blink_desc->led);
        
        led_blink_desc->is_running = false;
    }

    return;
}
#endif

#ifdef USE_PWM_LED
static void LED_API_Pulse_timer_Callback (void *arg) {
   sLedPulseDesc_t *led_pulse_desc = (sLedPulseDesc_t*) arg;

   if (!led_pulse_desc->is_running) {
       if (osMutexAcquire(led_pulse_desc->pulse_mutex, PULSE_MUTEX_TIMEOUT) != osOK) {
           return;
       }
   }

   led_pulse_desc->is_running = true;

   osMutexRelease(led_pulse_desc->pulse_mutex);

   PWM_Driver_Change_Duty_Cycle(g_pwm_led_control_static_lut[led_pulse_desc->led].pwm_device, led_pulse_desc->current_duty_cycle);

   if (led_pulse_desc->change_count >= led_pulse_desc->total_changes_per_pulse) {
       if (led_pulse_desc->count_dir_up) {
           led_pulse_desc->count_dir_up = false;
           led_pulse_desc->change_count = 0;
       } else {
           led_pulse_desc->count_dir_up = true;
           led_pulse_desc->change_count = 0;
           led_pulse_desc->pulse_count ++;
       }
   }

   if (led_pulse_desc->count_dir_up) {
       led_pulse_desc->current_duty_cycle += led_pulse_desc->duty_cycle_change;
   } else {
       led_pulse_desc->current_duty_cycle -= led_pulse_desc->duty_cycle_change;
   }

   led_pulse_desc->change_count++;

   if (led_pulse_desc->pulse_count >= led_pulse_desc->total_pulses) {
       osTimerStop(led_pulse_desc->pulse_timer);

       PWM_Driver_Change_Duty_Cycle(g_pwm_led_control_static_lut[led_pulse_desc->led].pwm_device, 0);

       led_pulse_desc->is_running = false;
   }

   return;
}
#endif

/**********************************************************************************************************************
 * Definitions of exported functions
 *********************************************************************************************************************/

bool LED_API_Init (void) {
    if (g_is_led_initialized || g_is_pwm_initialized) {
        return true;
    }

    if (!GPIO_Driver_InitAllPins()) {
        return false;
    }

    if (eLed_Last != 1) {
        g_is_led_initialized = true;
    }

    if (eLedPwm_Last != 1) {
        if (!PWM_Driver_InitAllDevices()) {
            return false;
        }

        g_is_pwm_initialized = true;
    }

    #ifdef USE_LED
    for (eLed_t led = (eLed_First + 1); led < eLed_Last; led++) {
        if (g_led_blink_lut[led].blink_timer == NULL) {
            g_led_blink_lut[led].blink_timer = osTimerNew(g_led_blink_lut[led].timer_callback, osTimerPeriodic, &g_led_blink_lut[led], &g_basic_led_control_static_lut[led].blink_timer_attributes);
        }

        if (g_led_blink_lut[led].blink_mutex == NULL) {
            g_led_blink_lut[led].blink_mutex = osMutexNew(&g_basic_led_control_static_lut[led].blink_mutex_attributes);
        }
    }
    #endif

    #ifdef USE_PWM_LED
    for (eLedPwm_t led = (eLedPwm_First + 1); led < eLedPwm_Last; led++) {
        if (g_led_pulse_lut[led].pulse_timer == NULL) {
            g_led_pulse_lut[led].pulse_timer = osTimerNew(g_led_pulse_lut[led].timer_callback, osTimerPeriodic, &g_led_pulse_lut[led], &g_pwm_led_control_static_lut[led].pulse_timer_attributes);
        }

        if (g_led_pulse_lut[led].pulse_mutex == NULL) {
            g_led_pulse_lut[led].pulse_mutex = osMutexNew(&g_pwm_led_control_static_lut[led].pulse_mutex_attributes);
        }

        if (!PWM_Driver_Enable_Device(g_pwm_led_control_static_lut[led].pwm_device)) {
            return false;
        }

        g_led_pulse_lut[led].timer_resolution = PWM_Driver_GetDeviceTimerResolution(g_pwm_led_control_static_lut[led].pwm_device);
    }
    #endif

    return true;
}

#ifdef USE_LED
bool LED_API_TurnOn (const eLed_t led) {
    if (!g_is_led_initialized) {
        return false;
    }
    
    if (!LED_API_IsCorrectLed(led)) {
        return false;
    }

    return GPIO_Driver_WritePin(g_basic_led_control_static_lut[led].led_pin, !g_basic_led_control_static_lut[led].is_inverted);
}

bool LED_API_TurnOff (const eLed_t led) {
    if (!g_is_led_initialized) {
        return false;
    }
    
    if (!LED_API_IsCorrectLed(led)) {
        return false;
    }
    
    return GPIO_Driver_WritePin(g_basic_led_control_static_lut[led].led_pin, g_basic_led_control_static_lut[led].is_inverted);
}

bool LED_API_Toggle (const eLed_t led) {
    if (!g_is_led_initialized) {
        return false;
    }
    
    if (!LED_API_IsCorrectLed(led)) {
        return false;
    }

    return GPIO_Driver_TogglePin(g_basic_led_control_static_lut[led].led_pin);
}

bool LED_API_Blink (const eLed_t led, const uint8_t blink_time, const uint16_t blink_frequency) {
    if (!g_is_led_initialized) {
        return false;
    }

    if (!LED_API_IsCorrectLed(led)) {
        return false;
    }

    if (!LED_API_IsCorrectBlinkTime(blink_time)) {
        return false;
    }

    if (!LED_API_IsCorrectBlinkFrequency(blink_frequency)) {
        return false;
    }

    if (g_led_blink_lut[led].is_running) {
        return true;
    }

    if (osMutexAcquire(g_led_blink_lut[led].blink_mutex, BLINK_MUTEX_TIMEOUT) != osOK) {
        return false;
    }

    g_led_blink_lut[led].total_blinks = (blink_time * 1000 / blink_frequency) * 2;
    g_led_blink_lut[led].blink_count = 0;

    osTimerStart(g_led_blink_lut[led].blink_timer, (blink_frequency / 2));

    osMutexRelease(g_led_blink_lut[led].blink_mutex);

    return true;
}
#endif

#ifdef USE_PWM_LED
bool LED_API_Set_Brightness (const eLedPwm_t led, const uint8_t brightness) {
    if (!g_is_pwm_initialized) {
        return false;
    }

    if (!LED_API_IsCorrectPwmLed(led)) {
        return false;
    }

    if (!LED_API_IsCorrectDutyCycle(led, brightness)) {
        return false;
    }

    return PWM_Driver_Change_Duty_Cycle(g_pwm_led_control_static_lut[led].pwm_device, brightness);
}

bool LED_API_Pulse (const eLedPwm_t led, const uint8_t pulsing_time, const uint16_t pulse_frequency) {
    if (!g_is_pwm_initialized) {
        return false;
    }

    if (!LED_API_IsCorrectPwmLed(led)) {
        return false;
    }

    if (!LED_API_IsCorrectPulseTime(pulsing_time)) {
        return false;
    }

    if (!LED_API_IsCorrectPulseFrequency(pulse_frequency)) {
        return false;
    }

    if (g_led_pulse_lut[led].is_running) {
        return false;
    }

    if (osMutexAcquire(g_led_pulse_lut[led].pulse_mutex, PULSE_MUTEX_TIMEOUT) != osOK) {
        return false;
    }

    g_led_pulse_lut[led].total_pulses = (pulsing_time * 1000 / pulse_frequency);
    g_led_pulse_lut[led].pulse_count = 0;

    g_led_pulse_lut[led].total_changes_per_pulse = pulse_frequency / 2; 
    g_led_pulse_lut[led].duty_cycle_change = g_led_pulse_lut[led].timer_resolution / g_led_pulse_lut[led].total_changes_per_pulse;
    
    g_led_pulse_lut[led].change_count = 0;
    g_led_pulse_lut[led].current_duty_cycle = g_led_pulse_lut[led].duty_cycle_change;
    g_led_pulse_lut[led].count_dir_up = true;

    osTimerStart(g_led_pulse_lut[led].pulse_timer, PULSE_TIMER_FREQUENCY);

    osMutexRelease(g_led_pulse_lut[led].pulse_mutex);

    return true;
}
#endif

#ifdef USE_LED
bool LED_API_IsCorrectLed (const eLed_t led) {
    return (led > eLed_First) && (led < eLed_Last);
}

bool LED_API_IsCorrectBlinkTime (const uint8_t blink_time) {
    return (blink_time <= MAX_BLINK_TIME) && (blink_time > 0);
}

bool LED_API_IsCorrectBlinkFrequency (const uint16_t blink_frequency) {
    return (blink_frequency <= MAX_BLINK_FREQUENCY) && (blink_frequency >= MIN_BLINK_FREQUENCY);
}
#endif

#ifdef USE_PWM_LED
bool LED_API_IsCorrectPwmLed (const eLedPwm_t led) {
    return (led > eLedPwm_First) && (led < eLedPwm_Last);
}

bool LED_API_IsCorrectDutyCycle (const eLedPwm_t led, const uint8_t duty_cycle) {
    return (duty_cycle >= 0) && (duty_cycle <= g_led_pulse_lut[led].timer_resolution);
}

bool LED_API_IsCorrectPulseTime (const uint8_t pulse_time) {
    return (pulse_time <= MAX_PULSING_TIME) && (pulse_time > 0);
}

bool LED_API_IsCorrectPulseFrequency (const uint16_t pulse_frequency) {
    return (pulse_frequency <= MAX_PULSE_FREQUENCY) && (pulse_frequency > MIN_PULSE_FREQUENCY);
}
#endif

#endif
