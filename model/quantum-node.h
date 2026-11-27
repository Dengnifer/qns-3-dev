/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef QUANTUM_NODE_H
#define QUANTUM_NODE_H

#include "ns3/address.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/ptr.h"

#include <map>
#include <string>

namespace ns3
{

class Node;
class QuantumErrorModel;
class QuantumMemory;
class QuantumPhyEntity;

/**
 * @ingroup quantum
 * The quantum side of a node: its owner name and its quantum memory.
 *
 * A QuantumNode is aggregated to an ordinary ns3::Node by
 * QuantumPhyEntity::AddNode(), usually through QuantumNetStackHelper. Any node
 * can therefore take part in a quantum network, however it was created, and is
 * reached from its node with GetObject<QuantumNode>().
 */
class QuantumNode : public Object
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    QuantumNode();

    /**
     * Constructor.
     *
     * @param qphyent The quantum physical entity the node belongs to.
     * @param owner Unique owner name of the node.
     */
    QuantumNode(Ptr<QuantumPhyEntity> qphyent, const std::string& owner);

    /**
     * Get the owner name.
     *
     * @return The owner name of the node.
     */
    std::string GetOwner() const;

    /**
     * Get the node this object is aggregated to.
     *
     * @return The node.
     */
    Ptr<Node> GetNode() const;

    /**
     * Get the quantum physical entity.
     *
     * @return The quantum physical entity the node belongs to.
     */
    Ptr<QuantumPhyEntity> GetQuantumPhyEntity() const;

    /**
     * Get the quantum memory.
     *
     * @return The memory holding the qubits the node owns.
     */
    Ptr<QuantumMemory> GetQuantumMemory() const;

    /**
     * Set the address at which the applications of other nodes reach this node.
     *
     * By default the node uses its first IPv4 address, or else its first
     * global IPv6 address.
     *
     * @param address An IPv4 or IPv6 address of the node.
     */
    void SetAddress(const Address& address);

    /**
     * Check whether the node can exchange classical messages.
     *
     * @return True if an address was set or the node has an IP address.
     */
    bool HasAddress() const;

    /**
     * Get the address at which the applications of other nodes reach this node.
     *
     * @return An IPv4 or IPv6 address of the node.
     */
    Address GetAddress() const;

    /**
     * Get the UDP port on which the node offers a service.
     *
     * The first request for a service reserves a port; later requests return
     * the same port. The helper of a receiving application and the helpers of
     * the applications that send to it can therefore run in any order. This is
     * used while a simulation is set up, not while it runs.
     *
     * @param service Name of the service; see QuantumChannel::GetServiceName().
     * @return The UDP port of the service.
     */
    uint16_t GetServicePort(const std::string& service);

    /**
     * Get the socket address on which the node offers a service.
     *
     * @param service Name of the service; see GetServicePort().
     * @return The address of the node combined with the UDP port of the service.
     */
    Address GetServiceAddress(const std::string& service);

    /**
     * Take ownership of a qubit.
     *
     * @param qubit Name of the qubit.
     */
    void AddQubit(const std::string& qubit);

    /**
     * Give up ownership of a qubit.
     *
     * @param qubit Name of the qubit.
     * @return True if the node owned the qubit.
     */
    bool RemoveQubit(const std::string& qubit);

    /**
     * Check whether the node owns a qubit.
     *
     * @param qubit Name of the qubit.
     * @return True if the node owns the qubit.
     */
    bool HasQubit(const std::string& qubit) const;

    /**
     * Set the memory noise of the qubits that the node generates from now on.
     *
     * @param coherenceTime Coherence time of the memory; see TimeModel.
     */
    void SetTimeModel(Time coherenceTime);

    /**
     * Set the noise of a gate applied by the node.
     *
     * @param gate Name of the gate.
     * @param coherenceTime Coherence time of the gate; see DephaseModel.
     */
    void SetDephaseModel(const std::string& gate, Time coherenceTime);

  protected:
    void DoDispose() override;

  private:
    /**
     * Look for the address of the node.
     *
     * @return The address set by the user, else the first IPv4 address, else
     *         the first global IPv6 address, else an invalid address.
     */
    Address FindAddress() const;

    /// First UDP port handed out by GetServicePort(), the start of the dynamic range.
    static constexpr uint16_t FIRST_PORT{49152};

    Ptr<QuantumPhyEntity> m_qphyent;                //!< The quantum physical entity
    std::string m_owner;                            //!< Owner name
    Ptr<QuantumMemory> m_memory;                    //!< Qubits the node owns
    Address m_address;                              //!< Address set by the user, if any
    uint16_t m_nextPort{FIRST_PORT};                //!< Next UDP port to hand out
    std::map<std::string, uint16_t> m_servicePorts; //!< UDP port of each service
};

} // namespace ns3

#endif /* QUANTUM_NODE_H */
