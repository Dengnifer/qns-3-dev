/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef TELEP_LIN_ADAPT_APP_H
#define TELEP_LIN_ADAPT_APP_H

#include "quantum-application.h"
#include "quantum-basis.h"

#include "ns3/nstime.h"

#include <string>
#include <utility>

namespace ns3
{

class QuantumChannel;
class Qubit;

/**
 * @ingroup quantum
 * One hop of a chain of teleportations with adapted control flow.
 *
 * One application runs on every node of the chain. A node that receives the
 * names of the qubits of its predecessor teleports the state onward. No node
 * measures: every node folds the flags of its predecessor into its own qubits
 * with controlled gates, and only the last node applies the corrections. One
 * run therefore covers all measurement branches of the whole chain, with the
 * timing of the classical messages preserved.
 *
 * The first node of the chain is the one with an Input attribute, and the last
 * node is the one without a QChannel attribute. *
 * The application computes no result itself. When its Finished trace fires,
 * read what you need from QuantumPhyEntity, for example with
 * PeekDensityMatrix() or CalculateFidelity().
 */
class TelepLinAdaptApp : public QuantumApplication
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

  protected:
    void DoDispose() override;

  private:
    void DoStartApplication() override;
    void HandleMessage(const std::string& message, const Address& from) override;

    /**
     * Teleport the qubit held by this node to the next node.
     */
    void Teleport();

    /**
     * Tell the next node the names of the qubits involved in this hop.
     */
    void Send();

    Ptr<QuantumChannel> m_channel;             //!< The quantum channel to the next node, if any
    std::pair<std::string, std::string> m_epr; //!< Bell pair to share with the next node
    std::pair<std::string, std::string> m_qubitsPred; //!< Flag qubits of the previous node
    std::pair<std::string, std::string> m_qubits; //!< Qubit to teleport and local half of the pair
    std::string m_qubit;                          //!< Half of the pair held by the next node
    Ptr<Qubit> m_input;                           //!< Qubit to teleport, at the first node
    Time m_delay; //!< Wait between sharing the pair and the local operations
};

} // namespace ns3

#endif /* TELEP_LIN_ADAPT_APP_H */
