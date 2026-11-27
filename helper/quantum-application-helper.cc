/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "quantum-application-helper.h"

#include "ns3/abort.h"
#include "ns3/node.h"
#include "ns3/pointer.h"
#include "ns3/quantum-node.h"
#include "ns3/quantum-phy-entity.h"
#include "ns3/string.h"

namespace ns3
{

QuantumApplicationHelper::QuantumApplicationHelper(TypeId typeId, Ptr<QuantumPhyEntity> qphyent)
    : ApplicationHelper(typeId),
      m_qphyent(qphyent)
{
    SetAttribute("QPhyEntity", PointerValue(qphyent));
}

ApplicationContainer
QuantumApplicationHelper::Install(Ptr<QuantumNode> node)
{
    return Install(node->GetNode());
}

ApplicationContainer
QuantumApplicationHelper::Install()
{
    NS_ABORT_MSG_IF(m_home.empty(), "This helper is not bound to a node; pass one to Install()");
    return Install(m_qphyent->GetQuantumNode(m_home));
}

void
QuantumApplicationHelper::SetHome(const std::string& owner)
{
    m_home = owner;
}

void
QuantumApplicationHelper::SetPeer(const std::string& owner, const std::string& service)
{
    SetAttribute("PeerOwner", StringValue(owner));
    SetAttribute("PeerService", StringValue(service));
}

void
QuantumApplicationHelper::SetService(const std::string& service)
{
    SetAttribute("Service", StringValue(service));
}

} // namespace ns3
