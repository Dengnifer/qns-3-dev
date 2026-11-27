/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "telep-adapt-helper.h"

#include "ns3/pointer.h"
#include "ns3/quantum-basis.h"
#include "ns3/quantum-channel.h"
#include "ns3/quantum-phy-entity.h"
#include "ns3/telep-adapt-app.h"

namespace ns3
{

TelepAdaptHelper::TelepAdaptHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumChannel> channel)
    : QuantumApplicationHelper(TelepAdaptApp::GetTypeId(), qphyent)
{
    SetAttribute("QChannel", PointerValue(channel));
    SetHome(channel->GetSrcOwner());
}

} // namespace ns3
