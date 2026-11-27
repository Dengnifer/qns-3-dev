/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "telep-lin-adapt-helper.h"

#include "ns3/pointer.h"
#include "ns3/quantum-basis.h"
#include "ns3/quantum-channel.h"
#include "ns3/quantum-phy-entity.h"
#include "ns3/telep-lin-adapt-app.h"

namespace ns3
{

TelepLinAdaptHelper::TelepLinAdaptHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumChannel> channel)
    : QuantumApplicationHelper(TelepLinAdaptApp::GetTypeId(), qphyent)
{
    // Every node of the chain listens on the same service.
    const std::string service = QNS_APP_TELEP + "_lin_adapt";
    SetService(service);
    if (channel)
    {
        SetAttribute("QChannel", PointerValue(channel));
        SetHome(channel->GetSrcOwner());
        SetPeer(channel->GetDstOwner(), service);
    }
}

} // namespace ns3
