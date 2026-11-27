/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "telep-helper.h"

#include "ns3/pointer.h"
#include "ns3/quantum-basis.h"
#include "ns3/quantum-channel.h"
#include "ns3/quantum-phy-entity.h"
#include "ns3/telep-app.h"
#include "ns3/uinteger.h"

namespace ns3
{

TelepSrcHelper::TelepSrcHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumChannel> channel)
    : QuantumApplicationHelper(TelepSrcApp::GetTypeId(), qphyent)
{
    SetAttribute("QChannel", PointerValue(channel));
    SetHome(channel->GetSrcOwner());
    SetPeer(channel->GetDstOwner(), channel->GetServiceName(QNS_APP_TELEP));
    // Payload size of the two-bit outcome message of the original implementation.
    SetAttribute("PacketSize", UintegerValue(1024));
}

TelepDstHelper::TelepDstHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumChannel> channel)
    : QuantumApplicationHelper(TelepDstApp::GetTypeId(), qphyent)
{
    SetHome(channel->GetDstOwner());
    SetService(channel->GetServiceName(QNS_APP_TELEP));
}

} // namespace ns3
