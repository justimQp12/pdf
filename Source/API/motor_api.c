/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include "motor_api.h"

#ifdef USE_MOTOR

#include <stdint.h>
#include "cmsis_os2.h"
#include "motor_driver.h"
#include "pwm_driver.h"
#include "timer_driver.h"
#include "gpio_driver.h"

/**********************************************************************************************************************
 * Private definitions and macros
 *********************************************************************************************************************/

#define INPUT_MIN_SPEED 0
#define INPUT_MAX_SPEED 100
#define MUTEX_TIMEOUT 0U

/**********************************************************************************************************************
 * Private typedef
 *********************************************************************************************************************/

typedef struct sMotorsRotation {
    eMotorRotation_t rotation[eMotorDirection_Last];
} sMotorsRotation_t;

typedef struct sMotorStatic {
    osMutexAttr_t mutex_attributes;
    osTimerAttr_t timer_attributes;
    void (*timer_callback) (void *arg);
    uint8_t motor_speed_offset;
} sMotorConst_t;

typedef struct sMotorDynamic {
    eMotorDriver_t motor;
    bool is_enabled;
    uint16_t speed;
    uint16_t new_speed;
    uint16_t max_speed;
    eMotorDirection_t direction;
    osMutexId_t mutex;
    bool is_soft_start_running;
    osTimerId_t soft_start_timer;
    size_t step_value;
    uint8_t soft_start_step;
} sMotorDynamic_t;

/**********************************************************************************************************************
 * Prototypes of private functions
 *********************************************************************************************************************/
 
static uint16_t Motor_API_Scale_Speed (const eMotor_t motor, const size_t speed);
static bool Motor_API_SetMotorSpeed (const eMotor_t motor, const size_t speed, const eMotorDirection_t direction);
static void Motor_API_Statup_TimerCallback (void *arg);

/**********************************************************************************************************************
 * Private constants
 *********************************************************************************************************************/

/* clang-format off */
const static sMotorsRotation_t g_static_motor_rotation_lut[eMotor_Last] = {
    #ifdef USE_MOTOR_A
    [eMotor_Right] = {
        .rotation = {
            [eMotorDirection_Forward] = eMotorRotation_Forward,
            [eMotorDirection_Reverse] = eMotorRotation_Backward,
            [eMotorDirection_Right] =  eMotorRotation_Backward,
            [eMotorDirection_Left] = eMotorRotation_Forward,
            [eMotorDirection_RightSoft] = eMotorRotation_Forward,
            [eMotorDirection_LeftSoft] = eMotorRotation_Forward
        }
    },
    #endif

    #ifdef USE_MOTOR_B
    [eMotor_Left] = {
        .rotation = {
            [eMotorDirection_Forward] = eMotorRotation_Forward,
            [eMotorDirection_Reverse] = eMotorRotation_Backward,
            [eMotorDirection_Right] = eMotorRotation_Forward,
            [eMotorDirection_Left] = eMotorRotation_Backward,
            [eMotorDirection_RightSoft] = eMotorRotation_Forward,
            [eMotorDirection_LeftSoft] = eMotorRotation_Forward
        }

    }
    #endif
};

const static sMotorConst_t g_static_motor_lut[eMotor_Last] = {
    #ifdef USE_MOTOR_A
    [eMotor_Right] = {
        .mutex_attributes = {.name = "Motor_A_Mutex", .attr_bits = osMutexRecursive | osMutexPrioInherit, .cb_mem = NULL, .cb_size = 0U},
        .timer_attributes = {.name = "Motor_A_Timer", .attr_bits = 0, .cb_mem = NULL, .cb_size = 0U},
        .timer_callback = Motor_API_Statup_TimerCallback,
        .motor_speed_offset = MOTOR_RIGHT_SPEED_OFFSET
    },
    #endif

    #ifdef USE_MOTOR_B
    [eMotor_Left] = {
        .mutex_attributes = {.name = "Motor_B_Mutex", .attr_bits = osMutexRecursive | osMutexPrioInherit, .cb_mem = NULL, .cb_size = 0U},
        .timer_attributes = {.name = "Motor_B_Timer", .attr_bits = 0, .cb_mem = NULL, .cb_size = 0U},
        .timer_callback = Motor_API_Statup_TimerCallback,
        .motor_speed_offset = MOTOR_LEFT_SPEED_OFFSET
    }
    #endif
}; 
/* clang-format on */

/**********************************************************************************************************************
 * Private variables
 *********************************************************************************************************************/

static bool g_is_all_motors_init = false;

/* clang-format off */
static sMotorDynamic_t g_dynamic_motor_lut[eMotor_Last] = {
    #ifdef USE_MOTOR_A
    [eMotor_Right] = {
        .motor = eMotorDriver_A,
        .is_enabled = false,
        .speed = 0,
        .new_speed = 0,
        .max_speed = 0,
        .direction = eMotorDirection_Last,
        .mutex = NULL,
        .is_soft_start_running = false,
        .soft_start_timer= NULL,
        .soft_start_step = 1,
    },
    #endif

    #ifdef USE_MOTOR_B
    [eMotor_Left] = {
        .motor = eMotorDriver_B,
        .is_enabled = false,
        .speed = 0,
        .new_speed = 0,
        .max_speed = 0,
        .direction = eMotorDirection_Last,
        .mutex = NULL,
        .is_soft_start_running = false,
        .soft_start_timer = NULL,
        .soft_start_step = 1,
    }
    #endif
};
/* clang-format on */

/**********************************************************************************************************************
 * Exported variables and references
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Definitions of private functions
 *********************************************************************************************************************/
 
static uint16_t Motor_API_Scale_Speed (const eMotor_t motor, const size_t speed) {
    if ((motor <= eMotor_First) || (motor >= eMotor_Last)) {
        return 0;
    }

    if (!Motor_API_IsCorrectSpeed(speed)) {
        return 0;
    }
    
    if (speed == INPUT_MIN_SPEED) {
        return 0;
    }

    if (speed == INPUT_MAX_SPEED) {
        return (uint16_t) ((uint32_t) MAX_SCALLED_SPEED * g_dynamic_motor_lut[motor].max_speed / INPUT_MAX_SPEED);
    }

    uint16_t scaled_speed = MIN_SCALLED_SPEED + ((speed * (MAX_SCALLED_SPEED - MIN_SCALLED_SPEED)) / MAX_SCALLED_SPEED);

    return (uint16_t) ((uint32_t) scaled_speed * g_dynamic_motor_lut[motor].max_speed / INPUT_MAX_SPEED);
}

static bool Motor_API_SetMotorSpeed (const eMotor_t motor, const size_t speed, const eMotorDirection_t direction) {
    if ((motor <= eMotor_First) || (motor >= eMotor_Last)) {
        return false;
    }

    if ((speed < STOP_SPEED) || (speed > g_dynamic_motor_lut[motor].max_speed)) {
        return false;
    }

    if (!Motor_API_IsCorrectDirection(direction)) {
        return false;
    }

    if (osMutexAcquire(g_dynamic_motor_lut[motor].mutex, MUTEX_TIMEOUT) != osOK) {
        return false;
    }

    switch (direction) {
        case eMotorDirection_Forward: {
            if (!Motor_Driver_SetSpeed(g_dynamic_motor_lut[motor].motor, g_static_motor_rotation_lut[motor].rotation[eMotorDirection_Forward], speed)) {
                return false;
            }
        } break;
        case eMotorDirection_Reverse: {
            if (!Motor_Driver_SetSpeed(g_dynamic_motor_lut[motor].motor, g_static_motor_rotation_lut[motor].rotation[eMotorDirection_Reverse], speed)) {
                return false;
            }
        } break;
        case eMotorDirection_Right: {
            if (!Motor_Driver_SetSpeed(g_dynamic_motor_lut[motor].motor, g_static_motor_rotation_lut[motor].rotation[eMotorDirection_Right], speed)) {
                return false;
            }
        } break;
        case eMotorDirection_Left: {
            if (!Motor_Driver_SetSpeed(g_dynamic_motor_lut[motor].motor, g_static_motor_rotation_lut[motor].rotation[eMotorDirection_Left], speed)) {
                return false;
            }
        } break;
        default: {
        } break;
    }

    osMutexRelease(g_dynamic_motor_lut[motor].mutex);

    return true;
}

static void Motor_API_Statup_TimerCallback (void *arg) {
    sMotorDynamic_t *motor_desc = (sMotorDynamic_t*) arg;

    uint16_t speed = motor_desc->step_value * motor_desc->soft_start_step;

    Motor_API_SetMotorSpeed(motor_desc->motor, speed, motor_desc->direction);

    motor_desc->soft_start_step++;

    if (motor_desc->soft_start_step >= MOTOR_SOFT_START_STEPS) {
        if (motor_desc->speed > speed) {
            Motor_API_SetMotorSpeed(motor_desc->motor, motor_desc->speed, motor_desc->direction);
        }

        motor_desc->soft_start_step = 1;
        motor_desc->is_soft_start_running = false;

        osTimerStop(motor_desc->soft_start_timer);
    }

    return;
}

/**********************************************************************************************************************
 * Definitions of exported functions
 *********************************************************************************************************************/

bool Motor_API_Init (void) {
    if (g_is_all_motors_init) {
        return true;
    }

    if (eMotor_Last == 1) {
        return false;
    }

    if (!GPIO_Driver_InitAllPins()) {
        return false;
    }

    if (!Timer_Driver_InitAllTimers()) {
        return false;
    }

    if (!PWM_Driver_InitAllDevices()) {
        return false;
    }

    if (!Motor_Driver_InitAllMotors()) {
        return false;
    }
    
    for (eMotor_t motor = (eMotor_First + 1); motor < eMotor_Last; motor++) {
        if (g_dynamic_motor_lut[motor].mutex == NULL) {
            g_dynamic_motor_lut[motor].mutex = osMutexNew(&g_static_motor_lut[motor].mutex_attributes);
        }

        if (g_dynamic_motor_lut[motor].soft_start_timer == NULL) {
            g_dynamic_motor_lut[motor].soft_start_timer = osTimerNew(g_static_motor_lut[motor].timer_callback, osTimerPeriodic, &g_dynamic_motor_lut[motor], &g_static_motor_lut[motor].timer_attributes);
        }

        if (!Motor_Driver_GetMaxSpeed(g_dynamic_motor_lut[motor].motor, &g_dynamic_motor_lut[motor].max_speed)) {
            return false;
        }
    }

    g_is_all_motors_init = true;

    return g_is_all_motors_init;
}

bool Motor_API_SetSpeed (const size_t speed, const eMotorDirection_t direction) {
    if (!Motor_API_IsCorrectSpeed(speed)) {
        return false;
    }

    if (!Motor_API_IsCorrectDirection(direction)) {
        return false;
    }

    for (eMotor_t motor = (eMotor_First + 1); motor < eMotor_Last; motor++) {
        if (!Motor_API_IsMotorEnabled(motor)) {
            if (!Motor_Driver_EnableMotor(motor)) {
                return false;
            }

            g_dynamic_motor_lut[motor].is_enabled = true;
        }

        if (g_dynamic_motor_lut[motor].is_soft_start_running) {
            return false;
        }

        if (osMutexAcquire(g_dynamic_motor_lut[motor].mutex, MUTEX_TIMEOUT) != osOK) {
            return false;
        }

        g_dynamic_motor_lut[motor].new_speed = Motor_API_Scale_Speed(motor, speed);

        if (direction == eMotorDirection_RightSoft && motor == eMotor_Left) {
            g_dynamic_motor_lut[motor].new_speed -= SOFT_TURN_SPEED_OFFSET;
        } else if (direction == eMotorDirection_LeftSoft && motor == eMotor_Right) {
            g_dynamic_motor_lut[motor].new_speed -= SOFT_TURN_SPEED_OFFSET;
        }

        if (g_dynamic_motor_lut[motor].new_speed == g_dynamic_motor_lut[motor].speed && g_dynamic_motor_lut[motor].direction == direction) {
            osMutexRelease(g_dynamic_motor_lut[motor].mutex);
            
            continue;
        }

        g_dynamic_motor_lut[motor].new_speed += g_static_motor_lut[motor].motor_speed_offset;

        if (g_dynamic_motor_lut[motor].new_speed > g_dynamic_motor_lut[motor].max_speed) {
            g_dynamic_motor_lut[motor].new_speed = g_dynamic_motor_lut[motor].max_speed;
        }

        g_dynamic_motor_lut[motor].direction = direction;

        if (g_dynamic_motor_lut[motor].speed == STOP_SPEED) {
            g_dynamic_motor_lut[motor].soft_start_step = 1;
            g_dynamic_motor_lut[motor].step_value = g_dynamic_motor_lut[motor].new_speed / MOTOR_SOFT_START_STEPS;
            g_dynamic_motor_lut[motor].is_soft_start_running = true;
        }

        osMutexRelease(g_dynamic_motor_lut[motor].mutex);

        if (g_dynamic_motor_lut[motor].speed == STOP_SPEED) {
            osTimerStart(g_dynamic_motor_lut[motor].soft_start_timer, MOTOR_SOFT_START_TIMER_PERIOD);
        } else {
            if (!Motor_API_SetMotorSpeed(motor, g_dynamic_motor_lut[motor].new_speed, g_dynamic_motor_lut[motor].direction)) {
                return false;
            }
        }

        g_dynamic_motor_lut[motor].speed = g_dynamic_motor_lut[motor].new_speed;
    }

    return true;
}

bool Motor_API_StopAllMotors (void) {
    for (eMotor_t motor = (eMotor_First + 1); motor < eMotor_Last; motor++) {
        if (!Motor_API_IsMotorEnabled(motor)) {
            return false;
        }

        g_dynamic_motor_lut[motor].speed = STOP_SPEED;

        if (!Motor_Driver_SetSpeed(g_dynamic_motor_lut[motor].motor, g_static_motor_rotation_lut[motor].rotation[eMotorDirection_Forward], g_dynamic_motor_lut[motor].speed)) {
            return false;
        }
    }

    return true;
}

bool Motor_API_EnableAllMotors (void) {
    for (eMotor_t motor = (eMotor_First + 1); motor < eMotor_Last; motor++) {
        if (Motor_API_IsMotorEnabled(motor)) {
            continue;
        }

        if (!Motor_Driver_EnableMotor(motor)) {
            return false;
        }

        g_dynamic_motor_lut[motor].is_enabled = true;
    }

    return true;
}

bool Motor_API_DisableAllMotors (void) {
    for (eMotor_t motor = (eMotor_First + 1); motor < eMotor_Last; motor++) {
        if (!Motor_API_IsMotorEnabled(motor)) {
            continue;
        }

        if (!Motor_Driver_DisableMotor(motor)) {
            return false;
        }

        g_dynamic_motor_lut[motor].is_enabled = false;
    }

    return true;
}

bool Motor_API_IsCorrectDirection (const eMotorDirection_t direction) {
    return (direction >= eMotorDirection_First) && (direction < eMotorDirection_Last);
}

bool Motor_API_IsCorrectSpeed (const size_t speed) {
    return (speed >= INPUT_MIN_SPEED) && (speed <= INPUT_MAX_SPEED);
}

bool Motor_API_IsMotorEnabled (const eMotor_t motor) {
    return g_dynamic_motor_lut[motor].is_enabled;
}

#endif
