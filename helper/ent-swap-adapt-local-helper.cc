/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ent-swap-adapt-local-helper.h"

#include "ns3/ent-swap-adapt-local-app.h"
#include "ns3/pointer.h"
#include "ns3/quantum-basis.h"
#include "ns3/quantum-channel.h"
#include "ns3/quantum-phy-entity.h"

namespace ns3
{

EntSwapAdaptLocalHelper::EntSwapAdaptLocalHelper(Ptr<QuantumPhyEntity> qphyent)
    : QuantumApplicationHelper(EntSwapAdaptLocalApp::GetTypeId(), qphyent)
{
}

} // namespace ns3
