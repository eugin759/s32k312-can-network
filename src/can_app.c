#include "can_app_config.h"
#include "can_app.h"

#include "Clock_Ip.h"
#include "OsIf_Cfg.h"
#include "OsIf_Timer_System_Internal_Systick.h"

volatile uint32 CanApp_RxCount;
volatile uint32 CanApp_TxCount;
volatile uint32 CanApp_LastRxId;
volatile uint8 CanApp_LastRxData[CAN_CLASSIC_DATA_LENGTH];
volatile CanHw_StatusType CanApp_LastHwStatus = CAN_HW_STATUS_SUCCESS;

static uint32 s_systickReference;
static uint64 s_elapsedCoreCycles;
static uint64 s_nextTransmitCycle;
static uint64 s_transmitPeriodCycles;
static uint8 s_sequenceNumber;
static uint32 s_coreClockHz;

static void CanApp_ProcessReceived(uint8 channel)
{
    Flexcan_Ip_MsgBuffType message;
    Flexcan_Ip_StatusType status;
    uint8 index;

    if (CanHw_TakeReceived(channel, &message) == TRUE)
    {
        CanApp_LastRxId = message.msgId;
        for (index = 0U; index < CAN_CLASSIC_DATA_LENGTH; index++)
        {
            CanApp_LastRxData[index] = (index < message.dataLen) ? message.data[index] : 0U;
        }
        CanApp_RxCount++;

        status = CanHw_ArmReceive(channel);
        if (status != FLEXCAN_STATUS_SUCCESS)
        {
            CanApp_LastHwStatus = CAN_HW_STATUS_FLEXCAN_ERROR;
        }
    }
}

static void CanApp_TransmitPeriodicFrame(void)
{
    uint8 data[CAN_CLASSIC_DATA_LENGTH] = {0U};
    uint32 messageId = NODE_TX_ID;
    Flexcan_Ip_StatusType status;

#if defined(INVALID_ID_TEST)
    messageId = TEST_INVALID_ID;
#endif

    data[0] = (uint8)NODE_ID;
    data[1] = s_sequenceNumber++;
    data[2] = (uint8)((NODE_TX_ID >> 8U) & 0x07U);
    data[3] = (uint8)(NODE_TX_ID & 0xFFU);
    data[4] = (uint8)(CanApp_LastRxId >> 8U);
    data[5] = (uint8)CanApp_LastRxId;
    data[6] = 0xA5U;
    data[7] = 0x5AU;

    status = CanHw_Transmit(messageId, data);
    if ((status != FLEXCAN_STATUS_SUCCESS) && (status != FLEXCAN_STATUS_BUSY))
    {
        CanApp_LastHwStatus = CAN_HW_STATUS_FLEXCAN_ERROR;
    }
}

boolean CanApp_Init(void)
{
    Flexcan_Ip_StatusType status;
    uint64 coreClockHz;

    coreClockHz = Clock_Ip_GetClockFrequency(CORE_CLK);
    if ((coreClockHz == 0U) || (coreClockHz > 0xFFFFFFFFULL))
    {
        return FALSE;
    }
    s_coreClockHz = (uint32)coreClockHz;
    s_transmitPeriodCycles = ((uint64)s_coreClockHz * CAN_TX_PERIOD_MS) / 1000U;
    if (s_transmitPeriodCycles == 0U)
    {
        return FALSE;
    }

    OsIf_Timer_System_Internal_Init(s_coreClockHz);
    s_systickReference = OsIf_Timer_System_Internal_GetCounter();
    s_elapsedCoreCycles = 0U;
    s_nextTransmitCycle = s_transmitPeriodCycles;

    status = CanHw_ArmReceive(CAN_HW_RX_CHANNEL_1);
    if (status != FLEXCAN_STATUS_SUCCESS)
    {
        return FALSE;
    }
    status = CanHw_ArmReceive(CAN_HW_RX_CHANNEL_2);
    if (status != FLEXCAN_STATUS_SUCCESS)
    {
        return FALSE;
    }
    return TRUE;
}

void CanApp_MainFunction(void)
{
    uint64 elapsedCycles;

    elapsedCycles = OsIf_Timer_System_Internal_GetElapsed(&s_systickReference);
    s_elapsedCoreCycles += elapsedCycles;

    CanHw_Service();
    CanApp_ProcessReceived(CAN_HW_RX_CHANNEL_1);
    CanApp_ProcessReceived(CAN_HW_RX_CHANNEL_2);
    CanApp_TxCount = CanHw_TxCompleteCount;

    if (s_elapsedCoreCycles >= s_nextTransmitCycle)
    {
        CanApp_TransmitPeriodicFrame();
        s_nextTransmitCycle += s_transmitPeriodCycles;
        if (s_nextTransmitCycle <= s_elapsedCoreCycles)
        {
            s_nextTransmitCycle = s_elapsedCoreCycles + s_transmitPeriodCycles;
        }
    }
}
