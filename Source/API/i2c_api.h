#ifndef SOURCE_API_I2C_API_H_
#define SOURCE_API_I2C_API_H_
/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "message.h"
#include "framework_config.h"

/**********************************************************************************************************************
 * Exported definitions and macros
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Exported types
 *********************************************************************************************************************/

/* clang-format off */
typedef enum eI2c {
    eI2c_First = 0,

    #ifdef USE_I2C1
    eI2c_1,
    #endif
    
    eI2c_Last
} eI2c_t;
/* clang-format on */

/**********************************************************************************************************************
 * Exported variables
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of exported functions
 *********************************************************************************************************************/

bool I2C_API_Init (const eI2c_t i2c);
bool I2C_API_Write (const eI2c_t i2c, const uint8_t device_address, uint8_t *data, const size_t data_size, const uint16_t mem_address, const uint8_t mem_address_size, uint32_t timeout);
bool I2C_API_Read (const eI2c_t i2c, const uint8_t device_address, uint8_t *data, const size_t data_size, const uint16_t mem_address, const uint8_t mem_address_size, uint32_t timeout);

#endif /* SOURCE_API_I2C_API_H_ */
