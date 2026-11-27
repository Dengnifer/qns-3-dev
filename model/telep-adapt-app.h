/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef TELEP_ADAPT_APP_H
#define TELEP_ADAPT_APP_H

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
 * Quantum teleportation with adapted control flow.
 *
 * Instead of measuring Alice's qubits and correcting Bob's qubit according to
 * the outcomes, the application applies the corrections as controlled gates
 * and traces Alice's qubits out. One run then covers all four measurement
 * branches, and the state of Bob's qubit is exact rather than sampled.
 *
 * The application runs on Alice's node and needs no classical message. *
 * The application computes no result itself. When its Finished trace fires,
 * read what you need from QuantumPhyEntity, for example with
 * PeekDensityMatrix() or CalculateFidelity().
 */
class TelepAdaptApp : public QuantumApplication
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

    Ptr<QuantumChannel> m_channel; //!< The quantum channel from Alice to Bob
    std::string m_lastOwner;       //!< Owner at the end of the teleportation chain
    std::pair<std::string, std::string>
        m_qubits;        //!< Alice's qubit to teleport and her half of the pair
    std::string m_qubit; //!< Bob's half of the pair
    Ptr<Qubit> m_input;  //!< State to prepare Alice's qubit in, if any
    Time m_delay;        //!< Wait between sharing the pair and the local operations
};

} // namespace ns3

#endif /* TELEP_ADAPT_APP_H */
