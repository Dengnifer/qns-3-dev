/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef ENT_SWAP_ADAPT_APP_H
#define ENT_SWAP_ADAPT_APP_H

#include "quantum-application.h"

namespace ns3
{

class QuantumMemory;

/**
 * @ingroup quantum
 * A chain of entanglement swappings with adapted control flow.
 *
 * Instead of measuring at every repeater node, the application accumulates
 * the X and Z flags of all repeater nodes in two ancilla qubits with
 * controlled gates, traces the repeater qubits out, and applies the
 * corrections of the last node controlled by the ancillas. One run covers all
 * measurement branches, and the final fidelity is exact.
 *
 * The application runs on the last node of the chain. Entry i of the two
 * memories names the qubits of node i: the one entangled with node i - 1 and
 * the one entangled with node i + 1. The Bell pairs must exist when the
 * application starts. *
 * The application computes no result itself. When its Finished trace fires,
 * read what you need from QuantumPhyEntity, for example with
 * PeekDensityMatrix() or CalculateFidelity().
 */
class EntSwapAdaptApp : public QuantumApplication
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
     * Run the whole chain of swappings.
     */
    void EntanglementSwapping();

    Ptr<QuantumMemory> m_qubitsFormer; //!< Per node, the qubit entangled with the previous node
    Ptr<QuantumMemory> m_qubitsLatter; //!< Per node, the qubit entangled with the next node
};

} // namespace ns3

#endif /* ENT_SWAP_ADAPT_APP_H */
