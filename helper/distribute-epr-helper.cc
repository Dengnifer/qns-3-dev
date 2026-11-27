/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "distribute-epr-helper.h"

#include "ns3/distribute-epr-protocol.h"
#include "ns3/pointer.h"
#include "ns3/quantum-basis.h"
#include "ns3/quantum-channel.h"
#include "ns3/quantum-phy-entity.h"

namespace ns3
{

DistributeEprSrcHelper::DistributeEprSrcHelper(Ptr<QuantumPhyEntity> qphyent,
                                               Ptr<QuantumChannel> channel)
    : QuantumApplicationHelper(DistributeEprSrcProtocol::GetTypeId(), qphyent)
{
    SetAttribute("QChannel", PointerValue(channel));
    SetHome(channel->GetSrcOwner());
    SetPeer(channel->GetDstOwner(), channel->GetServiceName(QNS_APP_DIST_EPR));
}

DistributeEprDstHelper::DistributeEprDstHelper(Ptr<QuantumPhyEntity> qphyent,
                                               Ptr<QuantumChannel> channel)
    : QuantumApplicationHelper(DistributeEprDstProtocol::GetTypeId(), qphyent)
{
    SetAttribute("QChannel", PointerValue(channel));
    SetHome(channel->GetDstOwner());
    SetService(channel->GetServiceName(QNS_APP_DIST_EPR));
}

} // namespace ns3
