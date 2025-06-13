#ifndef SOURCE_API_UART_API_H_
#define SOURCE_API_UART_API_H_
/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "uart_baudrate.h"
#include "message.h"
#include "framework_config.h"

/**********************************************************************************************************************
 * Exported definitions and macros
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Exported types
 *********************************************************************************************************************/

/* clang-format off */
typedef enum eUart {
    eUart_First = 0,

    #ifdef USE_UART_DEBUG
    eUart_Debug,
    #endif

    #ifdef USE_UART_UROS_TX
    eUart_uRos,
    #endif
    
    eUart_Last
} eUart_t;
/* clang-format on */

/**********************************************************************************************************************
 * Exported variables
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of exported functions
 *********************************************************************************************************************/

bool UART_API_Init (const eUart_t uart, const eUartBaudrate_t baudrate, const char *delimiter);
bool UART_API_Send (const eUart_t uart, const sMessage_t message, const uint32_t timeout);
bool UART_API_Receive (const eUart_t uart, sMessage_t *message, const uint32_t  timeout);

#endif /* SOURCE_API_UART_API_H_ */
