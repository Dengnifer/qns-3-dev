/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef DISTILL_NESTED_ADAPT_APP_H
#define DISTILL_NESTED_ADAPT_APP_H

#include "quantum-application.h"
#include "quantum-basis.h"

#include "ns3/nstime.h"

#include <string>
#include <vector>

namespace ns3
{

class QuantumChannel;
class QuantumMemory;

/**
 * @ingroup quantum
 * Nested entanglement distillation with adapted control flow.
 *
 * The rounds are those of DistillNestedSrcApp, but no round measures. Each
 * round computes the parity of its two outcomes into Bob's qubit with a
 * controlled gate and folds "the parity is even" into a flag qubit, which
 * starts as 1 and stays 1 only if every round succeeds. At the end, the
 * probability that the flag is 1 is the exact success probability of the
 * whole protocol, and the state conditioned on the flag is the exact
 * distilled pair. One run replaces the many sampled runs needed otherwise.
 *
 * The application runs on Alice's node and applies Bob's gates on his behalf,
 * with his noise. *
 * The application computes no result itself. When its Finished trace fires,
 * QuantumPhyEntity::PostSelect() of the flag qubit on outcome 1 returns the
 * success probability and conditions the state on success; after that,
 * QuantumPhyEntity::CalculateFidelity() of the first pair gives the fidelity
 * of the distilled pair.
 */
class DistillNestedAdaptApp : public QuantumApplication
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
     * Plan the distillation of some pairs into the first of them.
     *
     * @param srcQubits Alice's qubits of the pairs.
     * @param dstQubits Bob's qubits of the pairs.
     */
    void Distillate(const std::vector<std::string>& srcQubits,
                    const std::vector<std::string>& dstQubits);

    /**
     * Schedule one round: distill the first pair of each half.
     *
     * @param srcQubits Alice's qubits of the pairs.
     * @param dstQubits Bob's qubits of the pairs.
     */
    void DistillateOnce(const std::vector<std::string>& srcQubits,
                        const std::vector<std::string>& dstQubits);

    /**
     * Abort unless Bob has received qubits.
     *
     * @param qubits Names of the qubits that must have reached Bob.
     */
    void RequireArrived(const std::vector<std::string>& qubits) const;

    Ptr<QuantumChannel> m_channel;  //!< The quantum channel from Alice to Bob
    Ptr<QuantumMemory> m_srcQubits; //!< Alice's qubits, one per pair
    Ptr<QuantumMemory> m_dstQubits; //!< Bob's qubits, one per pair
    std::string m_flagQubit;        //!< Name of the flag qubit
    Time m_interval;                //!< Time allotted to the distribution of one pair
    Time m_occupied;                //!< Time until which the planned steps keep Alice busy
};

} // namespace ns3

#endif /* DISTILL_NESTED_ADAPT_APP_H */
