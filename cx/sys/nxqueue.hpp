// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

#include <FxObject.hpp>
#include <KArray.h>
#include <KPtr.h>
#include <NetClientQueue.h>
#include <NetPacketExtensionPrivate.h>

class NxAdapter;

#define QUEUE_CREATION_CONTEXT_SIGNATURE 0x7840dd95

struct QUEUE_CREATION_CONTEXT
{
    ULONG
        Signature = QUEUE_CREATION_CONTEXT_SIGNATURE;

    PKTHREAD
        CurrentThread = nullptr;

    NET_CLIENT_QUEUE_CONFIG const *
        ClientQueueConfig = nullptr;

    NET_CLIENT_QUEUE_NOTIFY_DISPATCH const *
        ClientDispatch = nullptr;

    NET_CLIENT_QUEUE_DISPATCH const **
        AdapterDispatch = nullptr;

    NxAdapter *
        Adapter = nullptr;

    Rtl::KArray<NET_PACKET_EXTENSION_PRIVATE>
        NetAdapterAddedPacketExtensions;

    Rtl::KArray<NET_PACKET_EXTENSION_PRIVATE>
        NetClientAddedPacketExtensions;

    ULONG
        QueueId = 0;

    void *
        ClientQueue = nullptr;

    wil::unique_wdf_object
        CreatedQueueObject;

};

class NxQueue
{
public:

    enum class Type
    {
        Rx,
        Tx,
    };

    NxQueue(
        _In_ QUEUE_CREATION_CONTEXT const & InitContext,
        _In_ ULONG QueueId,
        _In_ NET_PACKET_QUEUE_CONFIG const & QueueConfig,
        _In_ NxQueue::Type QueueType
    );

    virtual
    ~NxQueue(
        void
    ) = default;

    NTSTATUS
    Initialize(
        _In_ QUEUE_CREATION_CONTEXT & InitContext
    );

    static
    NTSTATUS
    NetQueueInitAddPacketExtension(
        _Inout_ QUEUE_CREATION_CONTEXT *CreationContext,
        _In_ const NET_PACKET_EXTENSION_PRIVATE* PacketExtension
    );

    void
    NotifyMorePacketsAvailable(
        void
    );

    size_t
        m_privateExtensionOffset = 0;

    size_t
        m_privateExtensionSize = 0;

    void
    Start(
        void
    );

    void
    Stop(
        void
    );

    void
    Advance(
        void
    );

    void
    Cancel(
        void
    );

    void
    SetArmed(
        _In_ bool isArmed
    );

    WDFOBJECT
    GetWdfObject(
        void
    );

    NET_RING_COLLECTION const *
    GetRingCollection(
        void
    ) const;

    NxAdapter *
    GetAdapter(
        void
    );

    void
    GetExtension(
        _In_ const NET_PACKET_EXTENSION_PRIVATE* ExtensionToQuery,
        _Out_ NET_EXTENSION* Extension
    );

    ULONG const
        m_queueId = ~0U;

    NxQueue::Type const
        m_queueType;

protected:

    NxAdapter *
        m_adapter = nullptr;

    Rtl::KArray<NET_PACKET_EXTENSION_PRIVATE>
        m_AddedPacketExtensions;

    NTSTATUS
    PrepareAndStorePacketExtensions(
        _In_ QUEUE_CREATION_CONTEXT & InitContext
    );

    NTSTATUS
    CreateRing(
        _In_ size_t ElementSize,
        _In_ UINT32 ElementCount,
        _In_ NET_RING_TYPE RingType
    );

private:

    KPoolPtr<NET_RING>
        m_rings[NET_RING_TYPE_FRAGMENT + 1];

    NET_RING_COLLECTION
        m_ringCollection;

    PVOID
        m_clientQueue = nullptr;

    NET_CLIENT_QUEUE_NOTIFY_DISPATCH const *
        m_clientDispatch = nullptr;

    NET_PACKET_QUEUE_CONFIG const
        m_packetQueueConfig;

protected:

    NETPACKETQUEUE
        m_queue = nullptr;
};

class NxTxQueue;

FORCEINLINE
NxTxQueue *
GetTxQueueFromHandle(
    _In_ NETPACKETQUEUE TxQueue
    );

class NxTxQueue :
    public NxQueue,
    public CFxObject<NETPACKETQUEUE, NxTxQueue, GetTxQueueFromHandle, true>
{
public:

    NxTxQueue(
        _In_ WDFOBJECT Object,
        _In_ QUEUE_CREATION_CONTEXT const & InitContext,
        _In_ ULONG QueueId,
        _In_ NET_PACKET_QUEUE_CONFIG const & QueueConfig
    );

    virtual
    ~NxTxQueue(
        void
    ) = default;

    NTSTATUS
    Initialize(
        _In_ QUEUE_CREATION_CONTEXT & InitContext
    );
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxTxQueue, _GetTxQueueFromHandle);

FORCEINLINE
NxTxQueue *
GetTxQueueFromHandle(
    NETPACKETQUEUE TxQueue
    )
{
    return _GetTxQueueFromHandle(TxQueue);
}

class NxRxQueue;

FORCEINLINE
NxRxQueue *
GetRxQueueFromHandle(
    _In_ NETPACKETQUEUE RxQueue
);

class NxRxQueue :
    public NxQueue,
    public CFxObject<NETPACKETQUEUE, NxRxQueue, GetRxQueueFromHandle, true>
{

public:

    NxRxQueue(
        _In_ WDFOBJECT Object,
        _In_ QUEUE_CREATION_CONTEXT const & InitContext,
        _In_ ULONG QueueId,
        _In_ NET_PACKET_QUEUE_CONFIG const & QueueConfig
    );

    virtual
    ~NxRxQueue(
        void
    ) = default;

    NTSTATUS
    Initialize(
        _In_ QUEUE_CREATION_CONTEXT & InitContext
    );
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NxRxQueue, _GetRxQueueFromHandle);

FORCEINLINE
NxRxQueue *
GetRxQueueFromHandle(
    NETPACKETQUEUE RxQueue
    )
{
    return _GetRxQueueFromHandle(RxQueue);
}

