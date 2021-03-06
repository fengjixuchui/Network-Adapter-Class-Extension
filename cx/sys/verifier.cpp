// Copyright (C) Microsoft Corporation. All rights reserved.

/*++

Abstract:
    Contains verifier function's implementations

--*/

#include "Nx.hpp"

#include "verifier.tmh"
#include "verifier.hpp"

#include <net/checksumtypes_p.h>
#include <net/lsotypes_p.h>

#include "NxAdapter.hpp"
#include "NxDevice.hpp"
#include "NxDriver.hpp"
#include "NxMacros.hpp"
#include "NxQueue.hpp"
#include "NxRequest.hpp"
#include "NxWake.hpp"
#include "version.hpp"

//
// If you change this behavior, or add new ones, make sure to update these comments.
//
// If you plan to add new Verifier functions, make sure to follow the pattern described bellow.
//
// Verifier_Verify* functions that use only VerifierAction_BugcheckAlways should not return any value.
// Verifier_Verify* functions that use VerifierAction_DbgBreakIfDebuggerPresent at least one time should return NTSTATUS, and the caller should deal with the status
//

VOID
Verifier_ReportViolation(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ VerifierAction      Action,
    _In_ FailureCode         FailureCode,
    _In_ ULONG_PTR           Parameter2,
    _In_ ULONG_PTR           Parameter3
)
/*++
Routine Description:
    This function takes failure parameters, and based on Action decides how to report it.
--*/
{
    switch (Action)
    {
        case VerifierAction_BugcheckAlways:
            NetAdapterCxBugCheck(
                PrivateGlobals,
                FailureCode,
                Parameter2,
                Parameter3);
            break;
        case VerifierAction_DbgBreakIfDebuggerPresent:
            if (!KdRefreshDebuggerNotPresent())
            {

                DbgBreakPoint();
            }
            break;
        default:
            break;
    }
}

VOID
Verifier_VerifyPrivateGlobals(
    NX_PRIVATE_GLOBALS * PrivateGlobals
)
{
    if (PrivateGlobals->Signature != NX_PRIVATE_GLOBALS_SIG)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_CorruptedPrivateGlobals,
            0,
            0);
    }
}

VOID
NetAdapterCxBugCheck(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ FailureCode         FailureCode,
    _In_ ULONG_PTR           Parameter2,
    _In_ ULONG_PTR           Parameter3
)
/*++
Routine Description:
    This function will bugcheck the system using NetAdapterCx bugcode. The first parameter
    is the failure code, followed by parameters 2 and 3 of the bugcheck. Parameter 4 will
    stay reserved in case we decide to create some sort of triage block.
--*/
{
    WdfCxVerifierKeBugCheck(
        PrivateGlobals->NxDriver->GetFxObject(),
        BUGCODE_NETADAPTER_DRIVER,
        FailureCode,
        Parameter2,
        Parameter3,
        0); // Reserved
}

VOID
Verifier_VerifyIrqlPassive(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals
)
{
    KIRQL currentIrql = KeGetCurrentIrql();

    if (currentIrql != PASSIVE_LEVEL)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_IrqlIsNotPassive,
            currentIrql,
            0);
    }
}

VOID
Verifier_VerifyIrqlLessThanOrEqualDispatch(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals
)
{
    KIRQL currentIrql = KeGetCurrentIrql();

    if (currentIrql > DISPATCH_LEVEL)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_IrqlNotLessOrEqualDispatch,
            currentIrql,
            0);
    }
}

VOID
Verifier_VerifyAdapterNotStarted(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NxAdapter *         pNxAdapter
)
{
    if (pNxAdapter->StartCalled())
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_AdapterAlreadyStarted,
            0,
            0);
    }
}

VOID
Verifier_VerifyNetPowerSettingsAccessible(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NxWake *             NetWake
)
{
    if (!NetWake->ArePowerSettingsAccessible())
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_EvtArmDisarmWakeNotInProgress,
            0,
            0);
    }
}

VOID
Verifier_VerifyNetRequestCompletionStatusNotPending(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NETREQUEST NetRequest,
    _In_ NTSTATUS   CompletionStatus
)
{
    if (CompletionStatus == STATUS_PENDING)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_CompletingNetRequestWithPendingStatus,
            (ULONG_PTR)NetRequest,
            0);
    }
}

VOID
Verifier_VerifyNetRequestType(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NxRequest * NxRequest,
    _In_ NDIS_REQUEST_TYPE Type
)
{
    NT_ASSERT(Type != NdisRequestQueryInformation && Type != NdisRequestQueryStatistics);

    if (NxRequest->m_NdisOidRequest->RequestType != Type)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetRequestType,
            NxRequest->m_NdisOidRequest->RequestType,
            0);
    }
}

VOID
Verifier_VerifyNetRequestIsQuery(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NxRequest * NxRequest
)
{
    if (NxRequest->m_NdisOidRequest->RequestType != NdisRequestQueryInformation &&
        NxRequest->m_NdisOidRequest->RequestType != NdisRequestQueryStatistics)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetRequestType,
            NxRequest->m_NdisOidRequest->RequestType,
            0);
    }
}

VOID
Verifier_VerifyNetRequest(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NxRequest * pNxRequest
)
{
    switch (pNxRequest->m_NdisOidRequest->RequestType)
    {
        case NdisRequestSetInformation:
            __fallthrough;
        case NdisRequestQueryInformation:
            __fallthrough;
        case NdisRequestQueryStatistics:
            __fallthrough;
        case NdisRequestMethod:
            break;
        default:
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidNetRequestType,
                pNxRequest->m_NdisOidRequest->RequestType,
                0);
            break;
    }
}

VOID
Verifier_VerifyNotNull(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ void const * const Ptr
)
{
    if (!Ptr)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_ParameterCantBeNull,
            0,
            0);
    }
}

NTSTATUS
Verifier_VerifyQueueConfiguration(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NET_REQUEST_QUEUE_CONFIG * QueueConfig
)
{
    NTSTATUS status = STATUS_SUCCESS;

    //
    // If this checks fail we always bugcheck the system
    //

    if (QueueConfig->Adapter == NULL)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidQueueConfiguration,
            0,
            0);
    }

    auto nxAdapter = GetNxAdapterFromHandle(QueueConfig->Adapter);

    if (QueueConfig->SizeOfSetDataHandler != sizeof(NET_REQUEST_QUEUE_SET_DATA_HANDLER))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidQueueConfiguration,
            QueueConfig->SizeOfSetDataHandler,
            sizeof(NET_REQUEST_QUEUE_SET_DATA_HANDLER));
    }

    if (QueueConfig->SizeOfQueryDataHandler != sizeof(NET_REQUEST_QUEUE_QUERY_DATA_HANDLER))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidQueueConfiguration,
            QueueConfig->SizeOfQueryDataHandler,
            sizeof(NET_REQUEST_QUEUE_QUERY_DATA_HANDLER));
    }

    if (QueueConfig->SizeOfMethodHandler != sizeof(NET_REQUEST_QUEUE_METHOD_HANDLER))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidQueueConfiguration,
            QueueConfig->SizeOfMethodHandler,
            sizeof(NET_REQUEST_QUEUE_METHOD_HANDLER));
    }

    //
    // Currently only DefaultSequential and DefaultParallel Queues can
    // be created, and only one of each can be created per NxAdapter
    // Verify that client isnt creating a duplicate queue.
    //

    switch (QueueConfig->Type)
    {
        case NetRequestQueueDefaultSequential:
            if (nxAdapter->m_DefaultRequestQueue != NULL)
            {
                Verifier_ReportViolation(
                    PrivateGlobals,
                    VerifierAction_BugcheckAlways,
                    FailureCode_DefaultRequestQueueAlreadyExists,
                    NetRequestQueueDefaultSequential,
                    0);
            }
            break;

        case NetRequestQueueDefaultParallel:
            if (nxAdapter->m_DefaultDirectRequestQueue != NULL)
            {
                Verifier_ReportViolation(
                    PrivateGlobals,
                    VerifierAction_BugcheckAlways,
                    FailureCode_DefaultRequestQueueAlreadyExists,
                    NetRequestQueueDefaultParallel,
                    0);
            }
            break;

        default:
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidRequestQueueType,
                QueueConfig->Type,
                0);
            break;
    }

    //
    // If these checks fail we return a NTSTATUS code
    //

    //
    // The Request queues allow the client to register handlers for varous requests
    // using the NET_REQUEST_QUEUE_CONFIG_ADD_XXX_HANDLER
    // The registration requires memory allocation and thus can fail. To keep the
    // client API simple, we don't return a failure in those APIs, we just
    // make a note of that error in the AddHandlerError bit field.
    // This routine fails the Queue Creation in case any bit of the
    // AddHandlerError is set.
    //
    if (QueueConfig->AddHandlerError.AsUchar != 0)
    {
        NET_REQUEST_QUEUE_ADD_HANDLER_ERROR error;

        error = QueueConfig->AddHandlerError;

        status = STATUS_UNSUCCESSFUL;

        if (error.AllocationFailed)
        {
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_DbgBreakIfDebuggerPresent,
                FailureCode_QueueConfigurationHasError,
                0, // Allocation failed
                0);

            status = STATUS_INSUFFICIENT_RESOURCES;
            error.AllocationFailed = 0;
        }

        if (error.CallbackNull)
        {
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_DbgBreakIfDebuggerPresent,
                FailureCode_QueueConfigurationHasError,
                1, // CallbackNull
                0);

            status = STATUS_INVALID_PARAMETER;
            error.CallbackNull = 0;
        }

        if (error.AsUchar != 0)
        {
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_DbgBreakIfDebuggerPresent,
                FailureCode_QueueConfigurationHasError,
                2, // Other error
                0);

            status = STATUS_UNSUCCESSFUL;
        }
    }

    return status;
}

VOID
Verifier_VerifyPowerCapabilities(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NxAdapter const & NxAdapter,
    _In_ NET_ADAPTER_POWER_CAPABILITIES const & PowerCapabilities
)
{
    if(!VERIFIER_CHECK_FLAGS(PowerCapabilities.Flags, NET_ADAPTER_POWER_CAPABILITIES_SUPPORTED_FLAGS))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidPowerCapabilities,
            0, // What field has invalid flags
            PowerCapabilities.Flags);
    }

    if (!VERIFIER_CHECK_FLAGS(PowerCapabilities.SupportedWakePatterns, NET_ADAPTER_WAKE_SUPPORTED_FLAGS))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidPowerCapabilities,
            1, // What field has invalid flags
            PowerCapabilities.SupportedWakePatterns);
    }

    if (!VERIFIER_CHECK_FLAGS(PowerCapabilities.SupportedProtocolOffloads, NET_ADAPTER_PROTOCOL_OFFLOADS_SUPPORTED_FLAGS))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidPowerCapabilities,
            2, // What field has invalid flags
            PowerCapabilities.SupportedProtocolOffloads);
    }

    if (!VERIFIER_CHECK_FLAGS(PowerCapabilities.SupportedWakeUpEvents, NET_ADAPTER_WAKEUP_SUPPORTED_FLAGS))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidPowerCapabilities,
            3, // What field has invalid flags
            PowerCapabilities.SupportedWakeUpEvents);
    }

    if (!VERIFIER_CHECK_FLAGS(PowerCapabilities.SupportedMediaSpecificWakeUpEvents, NET_ADAPTER_WAKEUP_MEDIA_SPECIFIC_SUPPORTED_FLAGS))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidPowerCapabilities,
            4, // What field has invalid flags
            PowerCapabilities.SupportedMediaSpecificWakeUpEvents);
    }

    auto const & previouslyReportedCapabilities = NxAdapter.m_PowerCapabilities;
    auto const startCalled = NxAdapter.StartCalled();

    if (startCalled &&
        previouslyReportedCapabilities.EvtAdapterPreviewWakePattern !=
            PowerCapabilities.EvtAdapterPreviewWakePattern)
    {
        //
        // Cannot alter EvtPreviewWakePattern after NetAdapterStart is called
        //
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidPowerCapabilities,
            5, // What field has invalid flags
            (ULONG_PTR)(PowerCapabilities.EvtAdapterPreviewWakePattern));
    }

    if (startCalled &&
        previouslyReportedCapabilities.EvtAdapterPreviewProtocolOffload !=
            PowerCapabilities.EvtAdapterPreviewProtocolOffload)
    {
        //
        // Cannot alter EvtPreviewProtocolOffload after NetAdapterStart is called
        //
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidPowerCapabilities,
            6, // What field has invalid flags
            (ULONG_PTR)(PowerCapabilities.EvtAdapterPreviewProtocolOffload));
    }


    if (startCalled &&
        previouslyReportedCapabilities.ManageS0IdlePowerReferences !=
            PowerCapabilities.ManageS0IdlePowerReferences)
    {
        //
        // Cannot alter EvtPreviewProtocolOffload after NetAdapterStart is called
        //
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidPowerCapabilities,
            7, // What field has invalid flags
            (ULONG_PTR)(PowerCapabilities.ManageS0IdlePowerReferences));
    }
}

VOID
Verifier_VerifyLinkLayerCapabilities(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NET_ADAPTER_LINK_LAYER_CAPABILITIES * LinkLayerCapabilities
)
{
    if (!VERIFIER_CHECK_FLAGS(LinkLayerCapabilities->SupportedStatistics, NET_ADAPTER_STATISTICS_SUPPORTED_FLAGS))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidLinkLayerCapabilities,
            1, // What field has invalid flags
            LinkLayerCapabilities->SupportedStatistics);
    }

    if (!VERIFIER_CHECK_FLAGS(LinkLayerCapabilities->SupportedPacketFilters, NET_PACKET_FILTER_SUPPORTED_FLAGS))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidLinkLayerCapabilities,
            0, // What field has invalid flags
            LinkLayerCapabilities->SupportedPacketFilters);
    }
}

VOID
Verifier_VerifyLinkLayerAddress(
    _In_ NX_PRIVATE_GLOBALS *PrivateGlobals,
    _In_ NET_ADAPTER_LINK_LAYER_ADDRESS *LinkLayerAddress
)
{
    if (LinkLayerAddress->Length > NDIS_MAX_PHYS_ADDRESS_LENGTH)
    {
        // The NIC driver tried to set a link layer address with a length larger
        // than the maximum allowed by NDIS
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_MacAddressLengthTooLong,
            LinkLayerAddress->Length,
            NDIS_MAX_PHYS_ADDRESS_LENGTH);
    }
}

VOID
Verifier_VerifyCurrentLinkState(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NET_ADAPTER_LINK_STATE * LinkState
)
{
    switch (LinkState->MediaConnectState)
    {
        case MediaConnectStateUnknown:
            __fallthrough;
        case MediaConnectStateConnected:
            __fallthrough;
        case MediaConnectStateDisconnected:
            break;
        default:
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidLinkState,
                0, // What field has invalid value
                (ULONG_PTR)LinkState->MediaConnectState);
            break;
    }

    switch (LinkState->MediaDuplexState)
    {
        case MediaDuplexStateUnknown:
            __fallthrough;
        case MediaDuplexStateHalf:
            __fallthrough;
        case MediaDuplexStateFull:
            break;
        default:
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidLinkState,
                1, // What field has invalid value
                (ULONG_PTR)LinkState->MediaDuplexState);
            break;
    }

    switch (LinkState->SupportedPauseFunctions)
    {
        case NetAdapterPauseFunctionsUnsupported:
            __fallthrough;
        case NetAdapterPauseFunctionsSendOnly:
            __fallthrough;
        case NetAdapterPauseFunctionsReceiveOnly:
            __fallthrough;
        case NetAdapterPauseFunctionsSendAndReceive:
            __fallthrough;
        case NetAdapterPauseFunctionsUnknown:
            break;
        default:
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidLinkState,
                2, // What field has invalid value
                (ULONG_PTR)LinkState->SupportedPauseFunctions);
            break;
    }

    if (!VERIFIER_CHECK_FLAGS(LinkState->AutoNegotiationFlags, NDIS_AUTO_NEGOTIATION_SUPPORTED_FLAGS))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidLinkState,
            3, // What field has invalid value
            (ULONG_PTR)LinkState->AutoNegotiationFlags);
    }
}

VOID
Verifier_VerifyQueryAsUlongFlags(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NET_CONFIGURATION_QUERY_ULONG_FLAGS Flags
)
{
    if (!VERIFIER_CHECK_FLAGS(Flags, NET_CONFIGURATION_QUERY_ULONG_SUPPORTED_FLAGS))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidQueryUlongFlag,
            Flags,
            0);
    }
}

VOID
Verifier_VerifyMtuSize(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
         ULONG MtuSize
)
{
    if (MtuSize == 0)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_MtuMustBeGreaterThanZero,
            0,
            0);
    }
}

VOID
Verifier_VerifyQueueInitContext(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ QUEUE_CREATION_CONTEXT *NetQueueInit
)
{
    if (NetQueueInit->Signature != QUEUE_CREATION_CONTEXT_SIGNATURE)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_BadQueueInitContext,
            0,
            0);
    }

    if (NetQueueInit->CurrentThread != KeGetCurrentThread())
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_CreatingNetQueueFromWrongThread,
            0,
            0);
    }

    if (NetQueueInit->CreatedQueueObject)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_QueueAlreadyCreated,
            (ULONG_PTR)NetQueueInit->CreatedQueueObject.get(),
            0);
    }
}

VOID
Verifier_VerifyNetPacketQueueConfiguration(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NET_PACKET_QUEUE_CONFIG const * Configuration
)
{
    // All Evt callbacks are required
    if (Configuration->EvtCancel == nullptr ||
        Configuration->EvtAdvance == nullptr ||
        Configuration->EvtSetNotificationEnabled == nullptr)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_NetQueueInvalidConfiguration,
            0,
            0);
    }
}

VOID
Verifier_VerifyObjectAttributesParentIsNull(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ WDF_OBJECT_ATTRIBUTES * ObjectAttributes
)
{
    if (ObjectAttributes != nullptr && ObjectAttributes->ParentObject != nullptr)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_ParentObjectNotNull,
            0,
            0);
    }
}

VOID
Verifier_VerifyObjectAttributesContextSize(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_opt_ WDF_OBJECT_ATTRIBUTES * ObjectAttributes,
    _In_ SIZE_T MaximumContextSize
)
{
    // FxValidateObjectAttributes checks most problems; we only add a maximum size check.

    size_t requestedSize = 0;

    if (ObjectAttributes != nullptr)
    {
        if (ObjectAttributes->ContextSizeOverride > 0)
        {
            requestedSize = ObjectAttributes->ContextSizeOverride;
        }
        else if (ObjectAttributes->ContextTypeInfo != nullptr)
        {
            requestedSize = ObjectAttributes->ContextTypeInfo->ContextSize;
        }
    }

    if (requestedSize > MaximumContextSize)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_ObjectAttributesContextSizeTooLarge,
            requestedSize,
            MaximumContextSize);
    }
}

VOID
Verifier_VerifyDatapathCallbacks(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NET_ADAPTER_DATAPATH_CALLBACKS const *DatapathCallbacks
)
{
    if (DatapathCallbacks->EvtAdapterCreateRxQueue == nullptr ||
        DatapathCallbacks->EvtAdapterCreateTxQueue == nullptr)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidDatapathCallbacks,
            (ULONG_PTR)DatapathCallbacks,
            0);
    }
}

VOID
Verifier_VerifyAdapterInitSignature(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ AdapterInit const *AdapterInit
)
{
    if (AdapterInit->InitSignature != ADAPTER_INIT_SIGNATURE)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetAdapterInitSignature,
            (ULONG_PTR)AdapterInit,
            0);
    }
}

VOID
Verifier_VerifyAdapterExtensionInitSignature(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ AdapterExtensionInit const *AdapterExtensionInit
)
{
    if (AdapterExtensionInit->InitSignature != ADAPTER_EXTENSION_INIT_SIGNATURE)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetAdapterExtensionInitSignature,
            (ULONG_PTR)AdapterExtensionInit,
            0);
    }
}

VOID
Verifier_VerifyAdapterInitNotUsed(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ AdapterInit const *AdapterInit
)
{
    if (AdapterInit->CreatedAdapter != nullptr)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_NetAdapterInitAlreadyUsed,
            (ULONG_PTR)AdapterInit,
            0);
    }
}

VOID
Verifier_VerifyReceiveScalingCapabilities(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NET_ADAPTER_RECEIVE_SCALING_CAPABILITIES const * Capabilities
)
{
    if (! RTL_IS_POWER_OF_TWO(Capabilities->IndirectionTableSize))
    {
        Verifier_ReportViolation(PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_NotPowerOfTwo,
            Capabilities->IndirectionTableSize,
            0);
    }

    if (! RTL_IS_POWER_OF_TWO(Capabilities->NumberOfQueues))
    {
        Verifier_ReportViolation(PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_NotPowerOfTwo,
            Capabilities->NumberOfQueues,
            0);
    }

    if (Capabilities->ReceiveScalingHashTypes >=
        (NetAdapterReceiveScalingHashTypeToeplitz << 1))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidReceiveScalingHashType,
            Capabilities->ReceiveScalingHashTypes,
            0);
    }

    if (Capabilities->ReceiveScalingProtocolTypes >=
        (NetAdapterReceiveScalingProtocolTypeUdp << 1))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidReceiveScalingProtocolType,
            Capabilities->ReceiveScalingProtocolTypes,
            0);
    }

    if (Capabilities->ReceiveScalingEncapsulationTypes.Outer >=
        (NetAdapterReceiveScalingEncapsulationTypeVXLan << 1))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidReceiveScalingEncapsulationType,
            Capabilities->ReceiveScalingEncapsulationTypes.Outer,
            0);
    }

    if (Capabilities->ReceiveScalingEncapsulationTypes.Inner >=
        (NetAdapterReceiveScalingEncapsulationTypeVXLan << 1))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidReceiveScalingEncapsulationType,
            Capabilities->ReceiveScalingEncapsulationTypes.Inner,
            0);
    }
}

VOID
Verifier_VerifyNetPacketExtensionName(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ PCWSTR ExtensionName
)
{
    if ((wcsstr(ExtensionName, L"ms_") == ExtensionName) &&
        (!(wcscmp(ExtensionName, NET_PACKET_EXTENSION_CHECKSUM_NAME) == 0 ||
           wcscmp(ExtensionName, NET_PACKET_EXTENSION_LSO_NAME) == 0)))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetPacketExtensionName,
            (ULONG_PTR)ExtensionName,
            0);
    }
}

VOID
Verifier_VerifyNetPacketExtensionVersionedSize(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NET_PACKET_EXTENSION const * NetPacketExtension
)
{
    bool reportVersionedSizeViolation = false;
    ULONG expectedSize = 0;

    if (wcscmp(NetPacketExtension->Name, NET_PACKET_EXTENSION_CHECKSUM_NAME) == 0)
    {
        switch (NetPacketExtension->Version)
        {
        case NET_PACKET_EXTENSION_CHECKSUM_VERSION_1:
            if (NetPacketExtension->ExtensionSize != NET_PACKET_EXTENSION_CHECKSUM_VERSION_1_SIZE)
            {
                expectedSize = NET_PACKET_EXTENSION_CHECKSUM_VERSION_1_SIZE;
                reportVersionedSizeViolation = true;
            }
            break;
        default:
            reportVersionedSizeViolation = true;
            break;
        }
    }

    if (wcscmp(NetPacketExtension->Name, NET_PACKET_EXTENSION_LSO_NAME) == 0)
    {
        switch (NetPacketExtension->Version)
        {
        case NET_PACKET_EXTENSION_LSO_VERSION_1:
            if (NetPacketExtension->ExtensionSize != NET_PACKET_EXTENSION_LSO_VERSION_1_SIZE)
            {
                expectedSize = NET_PACKET_EXTENSION_LSO_VERSION_1_SIZE;
                reportVersionedSizeViolation = true;
            }
            break;
        default:
            reportVersionedSizeViolation = true;
            break;
        }
    }

    if (reportVersionedSizeViolation)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_NetPacketExtensionVersionedSizeMismatch,
            (ULONG_PTR)NetPacketExtension,
            expectedSize);
    }
}

VOID
Verifier_VerifyNetPacketExtension(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NET_PACKET_EXTENSION const * NetPacketExtension
)
{
    if ((NetPacketExtension->Name == nullptr) ||
        (NetPacketExtension->Name[0] == L'\0'))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetPacketExtensionName,
            (ULONG_PTR)NetPacketExtension,
            0);
    }

    Verifier_VerifyNetPacketExtensionName(PrivateGlobals, NetPacketExtension->Name);

    if (NetPacketExtension->Version == 0)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetPacketExtensionVersion,
            (ULONG_PTR)NetPacketExtension,
            0);
    }

    if ((NetPacketExtension->Alignment == 0) ||
        ((NetPacketExtension->Alignment & (NetPacketExtension->Alignment + 1)) != 0) ||
        (NetPacketExtension->Alignment > alignof(NET_PACKET) - 1))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetPacketExtensionAlignment,
            (ULONG_PTR)NetPacketExtension,
            0);
    }

    if ((NetPacketExtension->ExtensionSize == 0) ||
        (NetPacketExtension->ExtensionSize > MAXIMUM_ALLOWED_EXTENSION_SIZE))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetPacketExtensionExtensionSize,
            (ULONG_PTR)NetPacketExtension,
            0);
    }

    Verifier_VerifyNetPacketExtensionVersionedSize(PrivateGlobals, NetPacketExtension);
}

VOID
Verifier_VerifyNetPacketExtensionQuery(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NET_PACKET_EXTENSION_QUERY const * NetPacketExtension
)
{
    if ((NetPacketExtension->Name == nullptr) ||
        (NetPacketExtension->Name[0] == L'\0'))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetPacketExtensionName,
            (ULONG_PTR)NetPacketExtension,
            0);
    }

    Verifier_VerifyNetPacketExtensionName(PrivateGlobals, NetPacketExtension->Name);

    if (NetPacketExtension->Version == 0)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidNetPacketExtensionVersion,
            (ULONG_PTR)NetPacketExtension,
            0);
    }
}

VOID
Verifier_VerifyNetAdapterTxCapabilities(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ const NET_ADAPTER_TX_CAPABILITIES *TxCapabilities
)
{
    // If the mapping requirement is DMA, the adapter has to provide the DMA capabilities
    if (TxCapabilities->MappingRequirement == NetMemoryMappingRequirementDmaMapped &&
        ((TxCapabilities->DmaCapabilities == nullptr) || (TxCapabilities->DmaCapabilities->DmaEnabler == nullptr)))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidAdapterTxCapabilities,
            (ULONG_PTR)TxCapabilities,
            1);
    }

    // If the mapping requirement is DMA, the adapter has to provide the DMA v3 inteface
    if (TxCapabilities->MappingRequirement == NetMemoryMappingRequirementDmaMapped)
    {
        PDMA_ADAPTER dmaAdapter =
            WdfDmaEnablerWdmGetDmaAdapter(TxCapabilities->DmaCapabilities->DmaEnabler,
                                          WdfDmaDirectionWriteToDevice);

        if (dmaAdapter->DmaOperations->GetDmaAdapterInfo == nullptr)
        {
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidAdapterTxCapabilities,
                (ULONG_PTR) TxCapabilities,
                2);
        }
    }

    if (TxCapabilities->MaximumNumberOfQueues == 0)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidAdapterTxCapabilities,
            (ULONG_PTR)TxCapabilities,
            3);
    }

    // A valid fragment alignment requirement is either the default value or in the format (2^n)
    if (TxCapabilities->FragmentBufferAlignment != NET_ADAPTER_FRAGMENT_DEFAULT_ALIGNMENT &&
        !RTL_IS_POWER_OF_TWO(TxCapabilities->FragmentBufferAlignment))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidAdapterTxCapabilities,
            (ULONG_PTR)TxCapabilities,
            4);
    }

    if ((TxCapabilities->FragmentRingNumberOfElementsHint > 0) &&
        !RTL_IS_POWER_OF_TWO(TxCapabilities->FragmentRingNumberOfElementsHint))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_NotPowerOfTwo,
            TxCapabilities->FragmentRingNumberOfElementsHint,
            5);
    }
}

VOID
Verifier_VerifyNetAdapterRxCapabilities(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ const NET_ADAPTER_RX_CAPABILITIES *RxCapabilities
)
{
    // If the adapter does not ask the OS to allocate receive fragments, then
    // EvtAdapterReturnRxBuffer must be set
    if (RxCapabilities->AllocationMode == NetRxFragmentBufferAllocationModeDriver)
    {
        if ((RxCapabilities->AttachmentMode != NetRxFragmentBufferAttachmentModeDriver) ||
            (RxCapabilities->EvtAdapterReturnRxBuffer == nullptr))
        {
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidAdapterRxCapabilities,
                (ULONG_PTR) RxCapabilities,
                0);
        }
    }

    // If the mapping requirement is DMA, the adapter has to provide the DMA capabilities
    if (RxCapabilities->MappingRequirement == NetMemoryMappingRequirementDmaMapped &&
        ((RxCapabilities->DmaCapabilities == nullptr) || (RxCapabilities->DmaCapabilities->DmaEnabler == nullptr)))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidAdapterRxCapabilities,
            (ULONG_PTR)RxCapabilities,
            1);
    }

    // If the mapping requirement is DMA, the adapter has to provide the DMA v3 inteface
    if (RxCapabilities->MappingRequirement == NetMemoryMappingRequirementDmaMapped)
    {
        PDMA_ADAPTER dmaAdapter =
            WdfDmaEnablerWdmGetDmaAdapter(RxCapabilities->DmaCapabilities->DmaEnabler,
                                          WdfDmaDirectionReadFromDevice);

        if (dmaAdapter->DmaOperations->GetDmaAdapterInfo == nullptr)
        {
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidAdapterRxCapabilities,
                (ULONG_PTR) RxCapabilities,
                2);
        }
    }

    if (RxCapabilities->MaximumNumberOfQueues == 0)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidAdapterRxCapabilities,
            (ULONG_PTR)RxCapabilities,
            3);
    }

    // A valid fragment alignment requirement is either the default value or in the format (2^n)
    if (RxCapabilities->FragmentBufferAlignment != NET_ADAPTER_FRAGMENT_DEFAULT_ALIGNMENT &&
        !RTL_IS_POWER_OF_TWO(RxCapabilities->FragmentBufferAlignment))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_NotPowerOfTwo,
            RxCapabilities->FragmentBufferAlignment,
            4);
    }

    if ((RxCapabilities->FragmentRingNumberOfElementsHint > 0) &&
        !RTL_IS_POWER_OF_TWO(RxCapabilities->FragmentRingNumberOfElementsHint))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_NotPowerOfTwo,
            RxCapabilities->FragmentRingNumberOfElementsHint,
            5);
    }
}

VOID
Verifier_VerifyDeviceAdapterCollectionIsEmpty(
    _In_ NX_PRIVATE_GLOBALS *PrivateGlobals,
    _In_ NxDevice const *Device,
    _In_ NxAdapterCollection const *AdapterCollection
)
{
    if (AdapterCollection->Count() > 0)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_RemovingDeviceWithAdapters,
            reinterpret_cast<ULONG_PTR>(Device),
            reinterpret_cast<ULONG_PTR>(AdapterCollection));
    }
}

VOID
Verifier_VerifyLsoCapabilities(
    _In_ NX_PRIVATE_GLOBALS *PrivateGlobals,
    _In_ NET_ADAPTER_OFFLOAD_LSO_CAPABILITIES const *LsoCapabilities
)
{
    if (LsoCapabilities->IPv4 || LsoCapabilities->IPv6)
    {
        if (LsoCapabilities->MaximumOffloadSize == 0)
        {
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidLsoCapabilities,
                LsoCapabilities->MaximumOffloadSize,
                1);
        }

        if (LsoCapabilities->MinimumSegmentCount == 0)
        {
            Verifier_ReportViolation(
                PrivateGlobals,
                VerifierAction_BugcheckAlways,
                FailureCode_InvalidLsoCapabilities,
                LsoCapabilities->MinimumSegmentCount,
                2);
        }
    }
}

void
Verifier_VerifyIsMediaExtension(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals
)
{
    auto const clientName = &PrivateGlobals->ClientDriverGlobals->DriverName[0];
    auto const maxClientNameCch = ARRAYSIZE(PrivateGlobals->ClientDriverGlobals->DriverName);

    // We only support MbbCx as of now
    if (_strnicmp(clientName, "mbbcx", maxClientNameCch) != 0)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_IllegalPrivateApiCall,
            0,
            0);
    }
}

void
Verifier_VerifyExtensionGlobals(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals
)
{
    Verifier_VerifyPrivateGlobals(PrivateGlobals);

    if (!PrivateGlobals->IsMediaExtension)
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_IllegalPrivateApiCall,
            1,
            0);
    }
}

void
Verifier_VerifyRxQueueHandle(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NETPACKETQUEUE NetRxQueue
)
{
    if (!GetRxQueueFromHandle(NetRxQueue))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidQueueHandle,
            0,
            0);
    }
}

void
Verifier_VerifyTxQueueHandle(
    _In_ NX_PRIVATE_GLOBALS * PrivateGlobals,
    _In_ NETPACKETQUEUE NetTxQueue
)
{
    if (!GetTxQueueFromHandle(NetTxQueue))
    {
        Verifier_ReportViolation(
            PrivateGlobals,
            VerifierAction_BugcheckAlways,
            FailureCode_InvalidQueueHandle,
            1,
            0);
    }
}
