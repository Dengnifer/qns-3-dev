/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ent-swap-adapt-helper.h"

#include "ns3/ent-swap-adapt-app.h"
#include "ns3/pointer.h"
#include "ns3/quantum-basis.h"
#include "ns3/quantum-channel.h"
#include "ns3/quantum-phy-entity.h"

namespace ns3
{

EntSwapAdaptHelper::EntSwapAdaptHelper(Ptr<QuantumPhyEntity> qphyent)
    : QuantumApplicationHelper(EntSwapAdaptApp::GetTypeId(), qphyent)
{
}

} // namespace ns3
