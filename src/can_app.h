#ifndef CAN_APP_H
#define CAN_APP_H

#include "can_app_config.h"
#include "can_hw.h"

extern volatile uint32 CanApp_RxCount;
extern volatile uint32 CanApp_TxCount;
extern volatile uint32 CanApp_LastRxId;
extern volatile uint8 CanApp_LastRxData[CAN_CLASSIC_DATA_LENGTH];
extern volatile CanHw_StatusType CanApp_LastHwStatus;

boolean CanApp_Init(void);
void CanApp_MainFunction(void);

#endif
