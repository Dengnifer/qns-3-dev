/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "quantum-net-stack-helper.h"

#include "distribute-epr-helper.h"

#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/quantum-basis.h"
#include "ns3/quantum-channel.h"
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
                               const std::vector<std::string>& owners,
                               Connectivity connectivity) const
{
    NS_LOG_FUNCTION(this << nodes.GetN());
    NS_ABORT_MSG_IF(nodes.GetN() != owners.size(),
                    nodes.GetN() << " node(s) given with " << owners.size() << " owner name(s)");

    Ptr<QuantumPhyEntity> qphyent = m_phyFactory.Create<QuantumPhyEntity>();
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        qphyent->AddNode(nodes.Get(i), owners[i]);
    }

    switch (connectivity)
    {
    case Connectivity::NONE:
        break;
    case Connectivity::CHAIN:
        for (std::size_t i = 0; i + 1 < owners.size(); ++i)
        {
            Connect(qphyent, owners[i], owners[i + 1]);
        }
        break;
    case Connectivity::FULL_MESH:
        for (const auto& src : owners)
        {
            for (const auto& dst : owners)
            {
                if (src != dst)
                {
                    Connect(qphyent, src, dst);
                }
            }
        }
        break;
    }
    return qphyent;
}

void
QuantumNetStackHelper::Connect(Ptr<QuantumPhyEntity> qphyent,
                               const std::string& srcOwner,
                               const std::string& dstOwner) const
{
    NS_LOG_FUNCTION(this << srcOwner << dstOwner);
    Ptr<QuantumChannel> channel = CreateObject<QuantumChannel>(srcOwner, dstOwner);
    if (qphyent->HasConn2Apps(channel, QNS_APP_DIST_EPR))
    {
        return;
    }
    ApplicationContainer src = DistributeEprSrcHelper(qphyent, channel).Install();
    ApplicationContainer dst = DistributeEprDstHelper(qphyent, channel).Install();
    qphyent->AddConn2Apps(channel, QNS_APP_DIST_EPR, {src.Get(0), dst.Get(0)});
}

} // namespace ns3
