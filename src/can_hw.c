#include "can_app_config.h"
#include "can_app.h"
#include "can_hw.h"

#include "Clock_Ip.h"
#include "Clock_Ip_Cfg.h"
#include "FlexCAN_Ip_Cfg.h"
#include "FlexCAN_Ip_HwAccess.h"
#include "FlexCAN_Ip_Irq.h"
#include "S32K312_SIUL2.h"
#include "Siul2_Port_Ip.h"
#include "Siul2_Port_Ip_Cfg.h"

#if defined(CAN_HW_EXTERNAL)
#if (FLEXCAN_IP_INSTANCE_COUNT < 2U)
#error "The RTD device configuration does not expose FlexCAN instance 1"
#endif
#endif

#define CAN_HW_TX_MB 0U
#define CAN_HW_RX_MB_1 1U
#define CAN_HW_RX_MB_2 2U
#define CAN_HW_RX_MB_COUNT 2U

static Flexcan_Ip_MsgBuffType s_rxMessages[CAN_HW_RX_MB_COUNT];
static volatile uint8 s_rxPendingMask;

volatile Flexcan_Ip_StatusType CanHw_LastFlexcanStatus = FLEXCAN_STATUS_SUCCESS;
volatile uint32 CanHw_LastErrorStatus;
volatile uint8 CanHw_TxErrorCounter;
volatile uint8 CanHw_RxErrorCounter;
volatile uint32 CanHw_TxCompleteCount;

static void CanHw_FlexcanCallback(uint8 instance,
                                  Flexcan_Ip_EventType eventType,
                                  uint32 bufferIndex,
                                  const Flexcan_Ip_StateType * flexcanState)
{
    (void)instance;
    (void)flexcanState;

    if (eventType == FLEXCAN_EVENT_RX_COMPLETE)
    {
        if (bufferIndex == CAN_HW_RX_MB_1)
        {
            s_rxPendingMask |= 1U;
        }
        else if (bufferIndex == CAN_HW_RX_MB_2)
        {
            s_rxPendingMask |= 2U;
        }
    }
    else if (eventType == FLEXCAN_EVENT_TX_COMPLETE)
    {
        CanHw_TxCompleteCount++;
    }
}

static void CanHw_FlexcanErrorCallback(uint8 instance,
                                       Flexcan_Ip_EventType eventType,
                                       uint32 errorStatus,
                                       const Flexcan_Ip_StateType * flexcanState)
{
    (void)instance;
    (void)eventType;
    (void)flexcanState;
    CanHw_LastErrorStatus = errorStatus;
}

static uint32 CanHw_GetRxHardwareMask(void)
{
    /*
     * CAN_RX_FILTER_MASK is a logical compare mask (1 = compare).
     * FlexCAN RXIMR uses the opposite polarity (1 = don't care).
     */
    return ((~(uint32)CAN_RX_FILTER_MASK & 0x7FFU) << FLEXCAN_IP_ID_STD_SHIFT);
}

CanHw_StatusType CanHw_Init(void)
{
    Flexcan_Ip_ConfigType config = FlexCAN_Config0;
    Flexcan_Ip_DataInfoType rxInfo;
    Flexcan_Ip_StatusType driverStatus;
    uint32 rxMask;

    if (Clock_Ip_Init(Clock_Ip_aClockConfig) != CLOCK_IP_SUCCESS)
    {
        return CAN_HW_STATUS_CLOCK_ERROR;
    }

    /* Regenerate this table for CAN0 if the board's internal pinmux requires it. */
    if (Siul2_Port_Ip_Init(
            NUM_OF_CONFIGURED_PINS_PortContainer_0_BOARD_InitPeripherals,
            g_pin_mux_InitConfigArr_PortContainer_0_BOARD_InitPeripherals) != SIUL2_PORT_SUCCESS)
    {
        return CAN_HW_STATUS_PIN_ERROR;
    }

    config.max_num_mb = 16U;
    config.is_rx_fifo_needed = FALSE;
    config.fd_enable = FALSE;
    config.bitRateSwitch = FALSE;
    config.flexcanMode = FLEXCAN_NORMAL_MODE;
#if (CAN_USE_RUNTIME_BITTIMING_PATCH == 1U)
    /* Encoded CTRL1 segments: 24 MHz / (2 * 24 time quanta) = 500 kbit/s. */
    config.bitrate.propSeg = 7U;
    config.bitrate.phaseSeg1 = 7U;
    config.bitrate.phaseSeg2 = 6U;
    config.bitrate.preDivider = 1U;
    config.bitrate.rJumpwidth = 1U;
#endif
    config.Callback = CanHw_FlexcanCallback;
    config.ErrorCallback = CanHw_FlexcanErrorCallback;

    driverStatus = FlexCAN_Ip_Init(CAN_FLEXCAN_INSTANCE, &FlexCAN_State0, &config);
    CanHw_LastFlexcanStatus = driverStatus;
    if (driverStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return CAN_HW_STATUS_FLEXCAN_ERROR;
    }

    driverStatus = FlexCAN_Ip_SetRxMaskType(CAN_FLEXCAN_INSTANCE, FLEXCAN_RX_MASK_INDIVIDUAL);
    CanHw_LastFlexcanStatus = driverStatus;
    if (driverStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return CAN_HW_STATUS_FLEXCAN_ERROR;
    }

    rxInfo.msg_id_type = FLEXCAN_MSG_ID_STD;
    rxInfo.data_length = CAN_CLASSIC_DATA_LENGTH;
#if (FLEXCAN_IP_FEATURE_HAS_FD == STD_ON)
    rxInfo.fd_enable = FALSE;
    rxInfo.fd_padding = 0U;
    rxInfo.enable_brs = FALSE;
#endif
    rxInfo.is_remote = FALSE;
    rxInfo.is_polling = FALSE;

    rxMask = CanHw_GetRxHardwareMask();
    driverStatus = FlexCAN_Ip_ConfigRxMb(CAN_FLEXCAN_INSTANCE, CAN_HW_RX_MB_1, &rxInfo, NODE_RX_ID_1);
    CanHw_LastFlexcanStatus = driverStatus;
    if (driverStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return CAN_HW_STATUS_FLEXCAN_ERROR;
    }
    driverStatus = FlexCAN_Ip_ConfigRxMb(CAN_FLEXCAN_INSTANCE, CAN_HW_RX_MB_2, &rxInfo, NODE_RX_ID_2);
    CanHw_LastFlexcanStatus = driverStatus;
    if (driverStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return CAN_HW_STATUS_FLEXCAN_ERROR;
    }
    driverStatus = FlexCAN_Ip_SetRxIndividualMask(CAN_FLEXCAN_INSTANCE, CAN_HW_RX_MB_1, rxMask);
    CanHw_LastFlexcanStatus = driverStatus;
    if (driverStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return CAN_HW_STATUS_FLEXCAN_ERROR;
    }
    driverStatus = FlexCAN_Ip_SetRxIndividualMask(CAN_FLEXCAN_INSTANCE, CAN_HW_RX_MB_2, rxMask);
    CanHw_LastFlexcanStatus = driverStatus;
    if (driverStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return CAN_HW_STATUS_FLEXCAN_ERROR;
    }

    driverStatus = FlexCAN_Ip_SetStartMode(CAN_FLEXCAN_INSTANCE);
    CanHw_LastFlexcanStatus = driverStatus;
    if (driverStatus != FLEXCAN_STATUS_SUCCESS)
    {
        return CAN_HW_STATUS_FLEXCAN_ERROR;
    }

    s_rxPendingMask = 0U;
    return CAN_HW_STATUS_SUCCESS;
}

Flexcan_Ip_StatusType CanHw_ArmReceive(uint8 channel)
{
    uint8 mbIndex;

    if (channel == CAN_HW_RX_CHANNEL_1)
    {
        mbIndex = CAN_HW_RX_MB_1;
    }
    else if (channel == CAN_HW_RX_CHANNEL_2)
    {
        mbIndex = CAN_HW_RX_MB_2;
    }
    else
    {
        return FLEXCAN_STATUS_BUFF_OUT_OF_RANGE;
    }

    CanHw_LastFlexcanStatus = FlexCAN_Ip_Receive(
        CAN_FLEXCAN_INSTANCE, mbIndex, &s_rxMessages[channel], FALSE);
    return CanHw_LastFlexcanStatus;
}

boolean CanHw_TakeReceived(uint8 channel, Flexcan_Ip_MsgBuffType * message)
{
    uint8 byteIndex;
    uint8 pendingBit;

    if ((message == NULL_PTR) || (channel >= CAN_HW_RX_MB_COUNT))
    {
        return FALSE;
    }

    pendingBit = (uint8)(1U << channel);
    if ((s_rxPendingMask & pendingBit) == 0U)
    {
        return FALSE;
    }

    message->cs = s_rxMessages[channel].cs;
    message->msgId = s_rxMessages[channel].msgId;
    message->dataLen = s_rxMessages[channel].dataLen;
    message->id_hit = s_rxMessages[channel].id_hit;
    message->time_stamp = s_rxMessages[channel].time_stamp;
    for (byteIndex = 0U; (byteIndex < message->dataLen) && (byteIndex < sizeof(message->data)); byteIndex++)
    {
        message->data[byteIndex] = s_rxMessages[channel].data[byteIndex];
    }
    s_rxPendingMask &= (uint8)(~pendingBit);
    return TRUE;
}

Flexcan_Ip_StatusType CanHw_Transmit(uint32 messageId, const uint8 * data)
{
    Flexcan_Ip_DataInfoType txInfo;

    if (data == NULL_PTR)
    {
        return FLEXCAN_STATUS_ERROR;
    }

    txInfo.msg_id_type = FLEXCAN_MSG_ID_STD;
    txInfo.data_length = CAN_CLASSIC_DATA_LENGTH;
#if (FLEXCAN_IP_FEATURE_HAS_FD == STD_ON)
    txInfo.fd_enable = FALSE;
    txInfo.fd_padding = 0U;
    txInfo.enable_brs = FALSE;
#endif
    txInfo.is_remote = FALSE;
    txInfo.is_polling = FALSE;

    CanHw_LastFlexcanStatus = FlexCAN_Ip_Send(
        CAN_FLEXCAN_INSTANCE, CAN_HW_TX_MB, &txInfo, messageId, data);
    return CanHw_LastFlexcanStatus;
}

boolean CanHw_IsButtonPressed(void)
{
    return (((IP_SIUL2->CAN_BUTTON_INPUT_REGISTER & CAN_BUTTON_INPUT_MASK) ==
             CAN_BUTTON_ACTIVE_LEVEL) ? TRUE : FALSE);
}

boolean CanHw_IsInvalidIdButtonPressed(void)
{
    return (((IP_SIUL2->CAN_INVALID_ID_BUTTON_REGISTER & CAN_INVALID_ID_BUTTON_MASK) ==
             CAN_BUTTON_ACTIVE_LEVEL) ? TRUE : FALSE);
}

void CanHw_SetRxIndicator(boolean enabled)
{
    uint8 outputLevel = enabled ? CAN_LED_ON_LEVEL : CAN_LED_OFF_LEVEL;

#if (NODE_ID == 1)
    IP_SIUL2->CAN_LED_GREEN_OUTPUT_REGISTER = outputLevel;
#elif (NODE_ID == 2)
    IP_SIUL2->CAN_LED_BLUE_OUTPUT_REGISTER = outputLevel;
#elif (NODE_ID == 3)
    IP_SIUL2->CAN_LED_RED_OUTPUT_REGISTER = outputLevel;
#else
    IP_SIUL2->CAN_LED_RED_OUTPUT_REGISTER = outputLevel;
    IP_SIUL2->CAN_LED_GREEN_OUTPUT_REGISTER = outputLevel;
    IP_SIUL2->CAN_LED_BLUE_OUTPUT_REGISTER = outputLevel;
#endif
}

void CanHw_Service(void)
{
    uint8 servicePass;

    /* The current startup vector table routes external IRQs to undefined_handler. */
    for (servicePass = 0U; servicePass < 3U; servicePass++)
    {
        FlexCAN_IRQHandler(CAN_FLEXCAN_INSTANCE, 0U, 31U, FALSE);
    }
    FlexCAN_Busoff_Error_IRQHandler(CAN_FLEXCAN_INSTANCE);
    CanHw_LastErrorStatus = FlexCAN_Ip_GetErrorStatus(CAN_FLEXCAN_INSTANCE);
    CanHw_TxErrorCounter = FlexCAN_Ip_GetControllerTxErrorCounter(CAN_FLEXCAN_INSTANCE);
    CanHw_RxErrorCounter = FlexCAN_Ip_GetControllerRxErrorCounter(CAN_FLEXCAN_INSTANCE);
}
