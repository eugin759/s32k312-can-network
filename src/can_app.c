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
#if defined(ARBITRATION_TEST)
static uint64 s_elapsedCoreCycles;
static uint64 s_nextTransmitCycle;
static uint64 s_transmitPeriodCycles;
#endif
static uint8 s_sequenceNumber;
static uint64 s_rxLedRemainingCycles;
static uint64 s_rxLedPulseCycles;
#if !defined(ARBITRATION_TEST)
typedef struct
{
    boolean candidateState;
    boolean stableState;
    uint64 stableCycles;
} CanApp_ButtonStateType;

static CanApp_ButtonStateType s_txButton;
static uint8 s_pendingNodeFrames;
#if defined(INVALID_ID_TEST)
static CanApp_ButtonStateType s_invalidIdButton;
static uint8 s_pendingInvalidIdFrames;
#endif
static uint64 s_buttonDebounceThresholdCycles;
#endif

volatile uint32 CanApp_ButtonPressCount;
volatile uint32 CanApp_InvalidIdPressCount;

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
        CanHw_SetRxIndicator(TRUE);
        s_rxLedRemainingCycles = s_rxLedPulseCycles;

        status = CanHw_ArmReceive(channel);
        if (status != FLEXCAN_STATUS_SUCCESS)
        {
            CanApp_LastHwStatus = CAN_HW_STATUS_FLEXCAN_ERROR;
        }
    }
}

static Flexcan_Ip_StatusType CanApp_TransmitFrame(uint32 messageId, uint8 buttonState)
{
    uint8 data[CAN_CLASSIC_DATA_LENGTH] = {0U};
    Flexcan_Ip_StatusType status;

    data[0] = (uint8)NODE_ID;
    data[1] = buttonState;
    data[2] = s_sequenceNumber;
    data[3] = (uint8)((NODE_TX_ID >> 8U) & 0x07U);
    data[4] = (uint8)(NODE_TX_ID & 0xFFU);
    data[5] = (uint8)CanApp_LastRxId;
    data[6] = 0xA5U;
    data[7] = 0x5AU;

    status = CanHw_Transmit(messageId, data);
    if (status == FLEXCAN_STATUS_SUCCESS)
    {
        s_sequenceNumber++;
    }
    else if (status != FLEXCAN_STATUS_BUSY)
    {
        CanApp_LastHwStatus = CAN_HW_STATUS_FLEXCAN_ERROR;
    }
    return status;
}

static void CanApp_UpdateRxIndicator(uint64 elapsedCycles)
{
    if (s_rxLedRemainingCycles > 0U)
    {
        if (elapsedCycles >= s_rxLedRemainingCycles)
        {
            s_rxLedRemainingCycles = 0U;
            CanHw_SetRxIndicator(FALSE);
        }
        else
        {
            s_rxLedRemainingCycles -= elapsedCycles;
        }
    }
}

#if !defined(ARBITRATION_TEST)
static boolean CanApp_DebouncedPress(CanApp_ButtonStateType * button,
                                     boolean sampledState,
                                     uint64 elapsedCycles)
{
    if (sampledState != button->candidateState)
    {
        button->candidateState = sampledState;
        button->stableCycles = 0U;
    }
    else if (button->candidateState != button->stableState)
    {
        if (elapsedCycles >= (s_buttonDebounceThresholdCycles - button->stableCycles))
        {
            button->stableCycles = s_buttonDebounceThresholdCycles;
        }
        else
        {
            button->stableCycles += elapsedCycles;
        }

        if (button->stableCycles >= s_buttonDebounceThresholdCycles)
        {
            button->stableState = button->candidateState;
            button->stableCycles = 0U;
            return (button->stableState == TRUE) ? TRUE : FALSE;
        }
    }
    return FALSE;
}

static void CanApp_QueueFrame(uint8 * pendingFrames)
{
    if (*pendingFrames < 0xFFU)
    {
        (*pendingFrames)++;
    }
    else
    {
        CanApp_LastHwStatus = CAN_HW_STATUS_FLEXCAN_ERROR;
    }
}

static void CanApp_TransmitPendingFrame(uint8 * pendingFrames, uint32 messageId)
{
    Flexcan_Ip_StatusType status = CanApp_TransmitFrame(messageId, 1U);

    if (status != FLEXCAN_STATUS_BUSY)
    {
        (*pendingFrames)--;
    }
}

static void CanApp_ProcessButtons(uint64 elapsedCycles)
{
    if (CanApp_DebouncedPress(&s_txButton, CanHw_IsButtonPressed(), elapsedCycles) == TRUE)
    {
        CanApp_ButtonPressCount++;
        CanApp_QueueFrame(&s_pendingNodeFrames);
    }

#if defined(INVALID_ID_TEST)
    if (CanApp_DebouncedPress(&s_invalidIdButton,
                              CanHw_IsInvalidIdButtonPressed(),
                              elapsedCycles) == TRUE)
    {
        CanApp_InvalidIdPressCount++;
        CanApp_QueueFrame(&s_pendingInvalidIdFrames);
    }

    if (s_pendingInvalidIdFrames > 0U)
    {
        CanApp_TransmitPendingFrame(&s_pendingInvalidIdFrames, TEST_INVALID_ID);
    }
    else if (s_pendingNodeFrames > 0U)
#else
    if (s_pendingNodeFrames > 0U)
#endif
    {
        CanApp_TransmitPendingFrame(&s_pendingNodeFrames, NODE_TX_ID);
    }
}
#endif

boolean CanApp_Init(void)
{
    Flexcan_Ip_StatusType status;
    uint64 coreClockHz;
#if defined(ARBITRATION_TEST)
    s_elapsedCoreCycles = 0U;
#endif

#if (CLOCK_IP_GET_FREQUENCY_API == STD_ON)
    coreClockHz = Clock_Ip_GetClockFrequency(CORE_CLK);
#else
    {
        const Clock_Ip_ConfiguredFrequencyType (*configuredFrequencies)[];
        uint8 frequencyIndex;

        /* The generated RTD config disables the live-frequency API; use its
         * configured clock table instead of hard-coding CORE_CLK. */
        coreClockHz = 0U;
        configuredFrequencies = Clock_Ip_aClockConfig[0U].ConfiguredFrequencies;
        if (configuredFrequencies != NULL_PTR)
        {
            for (frequencyIndex = 0U;
                 frequencyIndex < Clock_Ip_aClockConfig[0U].ConfigureFrequenciesCount;
                 frequencyIndex++)
            {
                if ((*configuredFrequencies)[frequencyIndex].Name == CORE_CLK)
                {
                    coreClockHz = (*configuredFrequencies)[frequencyIndex].ConfiguredFrequencyValue;
                    break;
                }
            }
        }
    }
#endif

    if ((coreClockHz == 0U) || (coreClockHz > 0xFFFFFFFFULL))
    {
        return FALSE;
    }
    s_rxLedPulseCycles =
        (coreClockHz * CAN_RX_LED_ON_TIME_MS) / 1000U;
    if (s_rxLedPulseCycles == 0U)
    {
        return FALSE;
    }

#if !defined(ARBITRATION_TEST)
    s_buttonDebounceThresholdCycles =
        (coreClockHz * CAN_BUTTON_DEBOUNCE_MS) / 1000U;
    if (s_buttonDebounceThresholdCycles == 0U)
    {
        return FALSE;
    }
#endif

#if defined(ARBITRATION_TEST)
    s_transmitPeriodCycles = (coreClockHz * CAN_TX_PERIOD_MS) / 1000U;
    if (s_transmitPeriodCycles == 0U)
    {
        return FALSE;
    }
#endif

    OsIf_Timer_System_Internal_Init((uint32)coreClockHz);
    s_systickReference = OsIf_Timer_System_Internal_GetCounter();
    s_rxLedRemainingCycles = 0U;
    CanHw_SetRxIndicator(FALSE);
#if defined(ARBITRATION_TEST)
    s_nextTransmitCycle = s_transmitPeriodCycles;
#else
    s_txButton.candidateState = CanHw_IsButtonPressed();
    s_txButton.stableState = s_txButton.candidateState;
    s_txButton.stableCycles = 0U;
    s_pendingNodeFrames = 0U;
#if defined(INVALID_ID_TEST)
    s_invalidIdButton.candidateState = CanHw_IsInvalidIdButtonPressed();
    s_invalidIdButton.stableState = s_invalidIdButton.candidateState;
    s_invalidIdButton.stableCycles = 0U;
    s_pendingInvalidIdFrames = 0U;
#endif
#endif

    status = CanHw_ArmReceive(CAN_HW_RX_CHANNEL_1);
    if (status != FLEXCAN_STATUS_SUCCESS)
    {
        return FALSE;
    }
#if !defined(CAN_DEBUG_MODE)
    status = CanHw_ArmReceive(CAN_HW_RX_CHANNEL_2);
    if (status != FLEXCAN_STATUS_SUCCESS)
    {
        return FALSE;
    }
#endif
    return TRUE;
}

void CanApp_MainFunction(void)
{
    uint64 elapsedCycles;

    elapsedCycles = OsIf_Timer_System_Internal_GetElapsed(&s_systickReference);
#if defined(ARBITRATION_TEST)
    s_elapsedCoreCycles += elapsedCycles;
#endif

    CanApp_UpdateRxIndicator(elapsedCycles);
    CanHw_Service();
    CanApp_ProcessReceived(CAN_HW_RX_CHANNEL_1);
#if !defined(CAN_DEBUG_MODE)
    CanApp_ProcessReceived(CAN_HW_RX_CHANNEL_2);
#endif
    CanApp_TxCount = CanHw_TxCompleteCount;

#if defined(ARBITRATION_TEST)
    if (s_elapsedCoreCycles >= s_nextTransmitCycle)
    {
        (void)CanApp_TransmitFrame(NODE_TX_ID, 0U);
        s_nextTransmitCycle += s_transmitPeriodCycles;
        if (s_nextTransmitCycle <= s_elapsedCoreCycles)
        {
            s_nextTransmitCycle = s_elapsedCoreCycles + s_transmitPeriodCycles;
        }
    }
#else
    CanApp_ProcessButtons(elapsedCycles);
#endif
}
