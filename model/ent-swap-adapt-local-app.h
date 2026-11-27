/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef ENT_SWAP_ADAPT_LOCAL_APP_H
#define ENT_SWAP_ADAPT_LOCAL_APP_H

#include "quantum-application.h"
#include "quantum-basis.h"

namespace ns3
{

class QuantumMemory;

/**
 * @ingroup quantum
 * A chain of entanglement swappings with adapted control flow and local flags.
 *
 * Like EntSwapAdaptApp, but the flags travel along the chain: every repeater
 * folds the flags of its predecessor into its own two qubits, so that each
 * controlled gate only joins neighboring nodes and the tensor network stays a
 * chain. This scales to much longer chains than EntSwapAdaptApp.
 *
 * The application creates the noisy Bell pairs itself, as Werner states, and
 * runs on the last node of the chain. Entry i of the two memories names the
 * qubits of node i: the one entangled with node i - 1 and the one entangled
 * with node i + 1. *
 * The application computes no result itself. When its Finished trace fires,
 * read what you need from QuantumPhyEntity, for example with
 * PeekDensityMatrix() or CalculateFidelity().
 */
class EntSwapAdaptLocalApp : public QuantumApplication
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
    double m_eprFidelity{0.95};        //!< Fidelity of each Bell pair of the chain
};

} // namespace ns3

#endif /* ENT_SWAP_ADAPT_LOCAL_APP_H */
