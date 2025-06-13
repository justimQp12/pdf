#ifndef SOURCE_API_MOTOR_API_H_
#define SOURCE_API_MOTOR_API_H_
/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include <stdbool.h>
#include <stddef.h>
#include "framework_config.h"

/**********************************************************************************************************************
 * Exported definitions and macros
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Exported types
 *********************************************************************************************************************/

/* clang-format off */
typedef enum eMotor {
    eMotor_First = 0,

    #ifdef USE_MOTOR_A
    eMotor_Right,
    #endif

    #ifdef USE_MOTOR_B
    eMotor_Left,
    #endif
    
    eMotor_Last
} eMotor_t;

typedef enum eMotorDirection {
    eMotorDirection_First,
    eMotorDirection_Forward = eMotorDirection_First,
    eMotorDirection_Reverse,
    eMotorDirection_Right,
    eMotorDirection_Left,
    eMotorDirection_RightSoft,
    eMotorDirection_LeftSoft,
    eMotorDirection_Last
} eMotorDirection_t;
/* clang-format on */

/**********************************************************************************************************************
 * Exported variables
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of exported functions
 *********************************************************************************************************************/

bool Motor_API_Init (void);
bool Motor_API_SetSpeed (const size_t speed, const eMotorDirection_t direction);
bool Motor_API_StopAllMotors (void);
bool Motor_API_EnableAllMotors (void);
bool Motor_API_DisableAllMotors (void);
bool Motor_API_IsCorrectDirection (const eMotorDirection_t direction);
bool Motor_API_IsCorrectSpeed (const size_t speed);
bool Motor_API_IsMotorEnabled (const eMotor_t motor);

#endif /* SOURCE_API_MOTOR_API_H_ */
