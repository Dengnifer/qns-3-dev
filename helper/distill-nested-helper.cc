/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "distill-nested-helper.h"

#include "ns3/distill-nested-app.h"
#include "ns3/pointer.h"
#include "ns3/quantum-basis.h"
#include "ns3/quantum-channel.h"
#include "ns3/quantum-phy-entity.h"

namespace ns3
{

DistillNestedSrcHelper::DistillNestedSrcHelper(Ptr<QuantumPhyEntity> qphyent,
                                               Ptr<QuantumChannel> channel)
    : QuantumApplicationHelper(DistillNestedSrcApp::GetTypeId(), qphyent)
{
    SetAttribute("QChannel", PointerValue(channel));
    SetHome(channel->GetSrcOwner());
    SetPeer(channel->GetDstOwner(), channel->GetServiceName(QNS_APP_DISTILL_NESTED));
}

DistillNestedDstHelper::DistillNestedDstHelper(Ptr<QuantumPhyEntity> qphyent,
                                               Ptr<QuantumChannel> channel)
    : QuantumApplicationHelper(DistillNestedDstApp::GetTypeId(), qphyent)
{
    SetHome(channel->GetDstOwner());
    SetService(channel->GetServiceName(QNS_APP_DISTILL_NESTED));
}

} // namespace ns3
