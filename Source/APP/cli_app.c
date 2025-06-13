/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/

#include "cli_app.h"

#ifdef ENABLE_CLI

#include <ctype.h>
#include "cmsis_os2.h"
#include "framework_cli_lut.h"
#include "cmd_api.h"
#include "uart_api.h"
#include "heap_api.h"
#include "debug_api.h"
#include "message.h"
#include "error_messages.h"

#ifdef INCLUDE_PROJECT_CLI
#include "project_cli_lut.h"
#endif

/**********************************************************************************************************************
 * Private definitions and macros
 *********************************************************************************************************************/

#define DEBUG_CLI_APP

/**********************************************************************************************************************
 * Private typedef
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Private constants
 *********************************************************************************************************************/

#ifdef DEBUG_CLI_APP
CREATE_MODULE_NAME (CLI_APP)
#else
CREATE_MODULE_NAME_EMPTY
#endif

const static osThreadAttr_t g_cli_thread_attributes = {
    .name = "CLI_APP_Thread",
    .stack_size = 256 * 4,
    .priority = (osPriority_t) osPriorityNormal
};

/**********************************************************************************************************************
 * Private variables
 *********************************************************************************************************************/

static bool g_is_initialized = false;

static osThreadId_t g_cli_thread_id = NULL;
static char g_response_buffer[RESPONSE_MESSAGE_CAPACITY];

static sMessage_t g_command = {.data = NULL, .size = 0};
static sMessage_t g_response = {.data = g_response_buffer, .size = RESPONSE_MESSAGE_CAPACITY};

/**********************************************************************************************************************
 * Exported variables and references
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Prototypes of private functions
 *********************************************************************************************************************/

static void CLI_APP_Thread (void *arg);

/**********************************************************************************************************************
 * Definitions of private functions
 *********************************************************************************************************************/

static void CLI_APP_Thread (void *arg) {
    while (true) {
        if (UART_API_Receive(eUart_Debug, &g_command, osWaitForever)) {
            if (CMD_API_FindCommand(g_command, &g_response, g_framework_cli_lut, eCliFrameworkCmd_Last)){
                Heap_API_Free(g_command.data);
                
                continue;
            }

            #ifdef INCLUDE_PROJECT_CLI
            if (CMD_API_FindCommand(g_command, &g_response, g_project_cli_lut, eCliProjectCmd_Last)){
                Heap_API_Free(g_command.data);
                
                continue;
            }
            #endif

            TRACE_ERR(g_response.data);
        }
    }

    osThreadYield();
}

/**********************************************************************************************************************
 * Definitions of exported functions
 *********************************************************************************************************************/

bool CLI_APP_Init (const eUartBaudrate_t baudrate) {
    #if  defined(ENABLE_CLI) && defined(DEBUG_CLI_APP)
    #else
    return false;
    #endif
    
    if (g_is_initialized) {
        return true;
    }
    
    if ((baudrate < eUartBaudrate_First) || (baudrate >= eUartBaudrate_Last)) {
        return false;
    }

    if (Heap_API_Init() == false) {
        return false;
    }

    if (Debug_API_Init(baudrate) == false) {
        return false;
    }

    if (g_cli_thread_id == NULL) {
        g_cli_thread_id = osThreadNew(CLI_APP_Thread, NULL, &g_cli_thread_attributes);
    }

    g_is_initialized = true;

    return g_is_initialized;
}

#endif
