/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "quantum-net-stack-helper.h"

#include "ns3/abort.h"
#include "ns3/distribute-epr-protocol.h"
#include "ns3/log.h"
#include "ns3/quantum-node.h"
#include "ns3/quantum-phy-entity.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QuantumNetStackHelper");

QuantumNetStackHelper::QuantumNetStackHelper()
{
    m_phyFactory.SetTypeId("ns3::QuantumPhyEntity");
}

void
QuantumNetStackHelper::SetPhyAttribute(const std::string& name, const AttributeValue& value)
{
    m_phyFactory.Set(name, value);
}

Ptr<QuantumPhyEntity>
QuantumNetStackHelper::Install(const NodeContainer& nodes,
                               const std::vector<std::string>& owners) const
{
    NS_LOG_FUNCTION(this << nodes.GetN());
    NS_ABORT_MSG_IF(nodes.GetN() != owners.size(),
                    nodes.GetN() << " node(s) given with " << owners.size() << " owner name(s)");

    Ptr<QuantumPhyEntity> qphyent = m_phyFactory.Create<QuantumPhyEntity>();
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        qphyent->AddNode(nodes.Get(i), owners[i]);
        nodes.Get(i)->AggregateObject(CreateObject<DistributeEprProtocol>());
    }
    return qphyent;
}

} // namespace ns3
