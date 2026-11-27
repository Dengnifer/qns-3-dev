/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "distill-helper.h"

#include "ns3/distill-app.h"
#include "ns3/pointer.h"
#include "ns3/quantum-basis.h"
#include "ns3/quantum-channel.h"
#include "ns3/quantum-phy-entity.h"

namespace ns3
{

DistillSrcHelper::DistillSrcHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumChannel> channel)
    : QuantumApplicationHelper(DistillSrcApp::GetTypeId(), qphyent)
{
    SetAttribute("QChannel", PointerValue(channel));
    SetHome(channel->GetSrcOwner());
    SetPeer(channel->GetDstOwner(), channel->GetServiceName(QNS_APP_DISTILL));
}

DistillDstHelper::DistillDstHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumChannel> channel)
    : QuantumApplicationHelper(DistillDstApp::GetTypeId(), qphyent)
{
    SetHome(channel->GetDstOwner());
    SetService(channel->GetServiceName(QNS_APP_DISTILL));
}

} // namespace ns3
