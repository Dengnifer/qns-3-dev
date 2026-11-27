/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef ENT_SWAP_APP_H
#define ENT_SWAP_APP_H

#include "quantum-application.h"

#include <string>
#include <utility>

namespace ns3
{

class QuantumChannel;

/**
 * @ingroup quantum
 * A repeater node of a chain of entanglement swappings.
 *
 * The node holds one qubit entangled with the previous node and one entangled
 * with the next node. It measures both in the Bell basis and sends the two
 * outcomes to the last node of the chain.
 */
class EntSwapSrcApp : public QuantumApplication
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

    /**
     * Measure the two qubits in the Bell basis and send the outcomes.
     */
    void MeasureAndSend();

    Ptr<QuantumChannel> m_channel; //!< From this node to the last node of the chain
    std::pair<std::string, std::string>
        m_qubits; //!< Qubits entangled with the previous and next node
};

/**
 * @ingroup quantum
 * The last node of a chain of entanglement swappings.
 *
 * The node collects the outcomes of all repeater nodes and then applies the
 * accumulated Pauli corrections to its qubit, which leaves it entangled with
 * the qubit of the first node.
 */
class EntSwapDstApp : public QuantumApplication
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

  private:
    void HandleMessage(const std::string& message, const Address& from) override;

    std::string m_qubit; //!< The qubit of this node
    uint32_t m_count{0}; //!< Number of repeater nodes still to report
    bool m_flagX{false}; //!< Whether an X correction is due
    bool m_flagZ{false}; //!< Whether a Z correction is due
};

} // namespace ns3

#endif /* ENT_SWAP_APP_H */
