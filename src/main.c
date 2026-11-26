/*==================================================================================================
* Project : RTD AUTOSAR 4.9
* Platform : CORTEXM
* Peripheral : S32K3XX
* Dependencies : none
*
* Autosar Version : 4.9.0
* Autosar Revision : ASR_REL_4_9_REV_0000
* Autosar Conf.Variant :
* SW Version : 7.0.0
* Build Version : S32K3_RTD_7_0_0_QLP03_D2512_ASR_REL_4_9_REV_0000_20251210
*
* Copyright 2020 - 2025 NXP
*
* NXP Confidential and Proprietary. This software is owned or controlled by NXP and may only be
*   used strictly in accordance with the applicable license terms. By expressly
*   accepting such terms or by downloading, installing, activating and/or otherwise
*   using the software, you are agreeing that you have read, and that you agree to
*   comply with and are bound by, such license terms. If you do not agree to be
*   bound by the applicable license terms, then you may not retain, install,
*   activate or otherwise use the software.
==================================================================================================*/

/**
*   @file main.c
*
*   @addtogroup main_module main module documentation
*   @{
*/

#ifdef __cplusplus
extern "C"{
#endif

/* Including necessary configuration files. */
#include "Mcal.h"
#include "can_app.h"

volatile int exit_code = 0;
/* User includes */

/*!
  \brief The main function for the project.
  \details The startup initialization sequence is the following:
 * - startup asm routine
 * - main()
*/
int main(void)
{
    CanHw_StatusType hwStatus = CanHw_Init();

    if (hwStatus != CAN_HW_STATUS_SUCCESS)
    {
        CanApp_LastHwStatus = hwStatus;
        exit_code = (int)hwStatus;
        return exit_code;
    }
    if (CanApp_Init() != TRUE)
    {
        CanApp_LastHwStatus = CAN_HW_STATUS_FLEXCAN_ERROR;
        exit_code = (int)CAN_HW_STATUS_FLEXCAN_ERROR;
        return exit_code;
    }

    for(;;)
    {
        CanApp_MainFunction();
    }
    return exit_code;
}

#ifdef __cplusplus
}
#endif

/** @} */
