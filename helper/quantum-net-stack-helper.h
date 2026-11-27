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
 * The helper creates the QuantumPhyEntity of the network, gives every node an
 * owner name and a quantum memory, and installs the applications that
 * distribute Bell pairs between nodes.
 */
class QuantumNetStackHelper
{
  public:
    /// Which pairs of nodes can distribute Bell pairs to each other.
    enum class Connectivity
    {
        NONE,     //!< No pair; see Connect()
        CHAIN,    //!< Each node to the next one in the container
        FULL_MESH //!< Every ordered pair of nodes
    };

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
     * @param nodes The nodes.
     * @param owners Unique owner names, one per node.
     * @param connectivity Which pairs of nodes can distribute Bell pairs.
     * @return The quantum physical entity of the new network.
     */
    Ptr<QuantumPhyEntity> Install(const NodeContainer& nodes,
                                  const std::vector<std::string>& owners,
                                  Connectivity connectivity = Connectivity::FULL_MESH) const;

    /**
     * Let one node distribute Bell pairs to another.
     *
     * Does nothing if the two nodes are connected already.
     *
     * @param qphyent The quantum physical entity of the network.
     * @param srcOwner Owner name of the node that creates the pairs.
     * @param dstOwner Owner name of the node that receives one qubit of each pair.
     */
    void Connect(Ptr<QuantumPhyEntity> qphyent,
                 const std::string& srcOwner,
                 const std::string& dstOwner) const;

  private:
    ObjectFactory m_phyFactory; //!< Creates the quantum physical entity
};

} // namespace ns3

#endif /* QUANTUM_NET_STACK_HELPER_H */
