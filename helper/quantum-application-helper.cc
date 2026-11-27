/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "quantum-application-helper.h"

#include "ns3/abort.h"
#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/pointer.h"
#include "ns3/quantum-node.h"
#include "ns3/quantum-phy-entity.h"

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
    m_peerOwner = owner;
    m_peerService = service;
}

void
QuantumApplicationHelper::SetService(const std::string& service)
{
    m_service = service;
}

Ptr<Application>
QuantumApplicationHelper::DoInstall(Ptr<Node> node)
{
    // Turn the names into socket addresses now, as the helpers of ns-3 do.
    if (!m_service.empty())
    {
        Ptr<QuantumNode> quantumNode = node->GetObject<QuantumNode>();
        NS_ABORT_MSG_UNLESS(quantumNode,
                            "A quantum application must be installed on a quantum node; "
                            "see QuantumNetStackHelper");
        m_factory.Set("Local", AddressValue(quantumNode->GetServiceAddress(m_service)));
    }
    if (!m_peerOwner.empty())
    {
        m_factory.Set(
            "Remote",
            AddressValue(m_qphyent->GetQuantumNode(m_peerOwner)->GetServiceAddress(m_peerService)));
    }
    return ApplicationHelper::DoInstall(node);
}

} // namespace ns3
