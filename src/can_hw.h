#ifndef CAN_HW_H
#define CAN_HW_H

#include "FlexCAN_Ip.h"

typedef enum
{
    CAN_HW_STATUS_SUCCESS = 0U,
    CAN_HW_STATUS_CLOCK_ERROR,
    CAN_HW_STATUS_PIN_ERROR,
    CAN_HW_STATUS_FLEXCAN_ERROR
} CanHw_StatusType;

#define CAN_HW_RX_CHANNEL_1 0U
#define CAN_HW_RX_CHANNEL_2 1U

extern volatile Flexcan_Ip_StatusType CanHw_LastFlexcanStatus;
extern volatile uint32 CanHw_LastErrorStatus;
extern volatile uint8 CanHw_TxErrorCounter;
extern volatile uint8 CanHw_RxErrorCounter;
extern volatile uint32 CanHw_TxCompleteCount;

CanHw_StatusType CanHw_Init(void);
Flexcan_Ip_StatusType CanHw_ArmReceive(uint8 channel);
boolean CanHw_TakeReceived(uint8 channel, Flexcan_Ip_MsgBuffType * message);
Flexcan_Ip_StatusType CanHw_Transmit(uint32 messageId, const uint8 * data);
boolean CanHw_IsButtonPressed(void);
boolean CanHw_IsInvalidIdButtonPressed(void);
void CanHw_SetRxIndicator(boolean enabled);
void CanHw_Service(void);

#endif
