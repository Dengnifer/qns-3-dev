/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "distill-nested-adapt-helper.h"

#include "ns3/distill-nested-adapt-app.h"
#include "ns3/pointer.h"
#include "ns3/quantum-basis.h"
#include "ns3/quantum-channel.h"
#include "ns3/quantum-phy-entity.h"

namespace ns3
{

DistillNestedAdaptHelper::DistillNestedAdaptHelper(Ptr<QuantumPhyEntity> qphyent,
                                                   Ptr<QuantumChannel> channel)
    : QuantumApplicationHelper(DistillNestedAdaptApp::GetTypeId(), qphyent)
{
    SetAttribute("QChannel", PointerValue(channel));
    SetHome(channel->GetSrcOwner());
}

} // namespace ns3
