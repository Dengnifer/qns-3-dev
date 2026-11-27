/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef QUANTUM_NET_STACK_HELPER_H
#define QUANTUM_NET_STACK_HELPER_H

#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/ptr.h"

#include <string>
#include <vector>

namespace ns3
{

class QuantumPhyEntity;

/**
 * @ingroup quantum
 * Turns ordinary nodes into the nodes of a quantum network.
 *
 * The helper creates the QuantumPhyEntity of the network and installs the
 * quantum stack on every node: an owner name, a quantum memory, and the
 * service that distributes Bell pairs.
 */
class QuantumNetStackHelper
{
  public:
    QuantumNetStackHelper();

    /**
     * Set an attribute of the QuantumPhyEntity that Install() creates.
     *
     * @param name Name of the attribute.
     * @param value Value of the attribute.
     */
    void SetPhyAttribute(const std::string& name, const AttributeValue& value);

    /**
     * Make the nodes a quantum network.
     *
     * Every node gets a QuantumNode, which holds its owner name and its quantum
     * memory, and a DistributeEprProtocol, which distributes Bell pairs.
     *
     * @param nodes The nodes.
     * @param owners Unique owner names, one per node.
     * @return The quantum physical entity of the new network.
     */
    Ptr<QuantumPhyEntity> Install(const NodeContainer& nodes,
                                  const std::vector<std::string>& owners) const;

  private:
    ObjectFactory m_phyFactory; //!< Creates the quantum physical entity
};

} // namespace ns3

#endif /* QUANTUM_NET_STACK_HELPER_H */
