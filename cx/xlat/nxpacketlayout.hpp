// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

_Success_(return)
bool
NxGetPacketEtherType(
    _In_ NET_RING_COLLECTION const *descriptor,
    _In_ NET_PACKET const *packet,
    _Out_ USHORT *ethertype);

NET_PACKET_LAYOUT
NxGetPacketLayout(
    _In_ NDIS_MEDIUM mediaType,
    _In_ NET_RING_COLLECTION const *descriptor,
    _In_ NET_PACKET const *packet);
