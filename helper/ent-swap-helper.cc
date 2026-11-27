/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ent-swap-helper.h"

#include "ns3/ent-swap-app.h"
#include "ns3/pointer.h"
#include "ns3/quantum-basis.h"
#include "ns3/quantum-channel.h"
#include "ns3/quantum-node.h"
#include "ns3/quantum-phy-entity.h"
#include "ns3/uinteger.h"

namespace ns3
{

EntSwapSrcHelper::EntSwapSrcHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumChannel> channel)
    : QuantumApplicationHelper(EntSwapSrcApp::GetTypeId(), qphyent)
{
    SetAttribute("QChannel", PointerValue(channel));
    SetHome(channel->GetSrcOwner());
    SetPeer(channel->GetDstOwner(), QNS_APP_ENT_SWAP);
    // Payload size of the two-bit outcome message of the original implementation.
    SetAttribute("PacketSize", UintegerValue(1024));
}

EntSwapDstHelper::EntSwapDstHelper(Ptr<QuantumPhyEntity> qphyent, Ptr<QuantumNode> node)
    : QuantumApplicationHelper(EntSwapDstApp::GetTypeId(), qphyent)
{
    SetHome(node->GetOwner());
    SetService(QNS_APP_ENT_SWAP);
}

} // namespace ns3
