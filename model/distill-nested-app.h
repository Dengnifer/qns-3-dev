/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef DISTILL_NESTED_APP_H
#define DISTILL_NESTED_APP_H

#include "quantum-application.h"

#include "ns3/nstime.h"

#include <string>
#include <vector>

namespace ns3
{

class QuantumChannel;
class QuantumMemory;

/**
 * @ingroup quantum
 * Source side (Alice) of nested entanglement distillation.
 *
 * Starting from 2^k Bell pairs, the pairs are distilled two by two, then the
 * survivors two by two, and so on, until one pair is left; see DistillSrcApp
 * for one round. Alice plans all rounds when she starts and runs them one
 * after another.
 *
 * Each round samples its measurement outcomes. The protocol does not stop when
 * a round fails, so the final pair is only meaningful if GetWin() is true. For
 * the exact success probability and fidelity, see DistillNestedAdaptApp.
 */
class DistillNestedSrcApp : public QuantumApplication
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * Get the number of rounds whose verdict has arrived.
     *
     * @return The number of finished rounds.
     */
    uint32_t GetNRoundsDone() const;

    /**
     * Get the verdict.
     *
     * @return True if at least one round is over and no round has failed.
     */
    bool GetWin() const;

  protected:
    void DoDispose() override;

  private:
    void DoStartApplication() override;
    void HandleMessage(const std::string& message, const Address& from) override;

    /**
     * Plan the distillation of some pairs into the first of them.
     *
     * @param srcQubits Alice's qubits of the pairs.
     * @param dstQubits Bob's qubits of the pairs.
     */
    void Distillate(const std::vector<std::string>& srcQubits,
                    const std::vector<std::string>& dstQubits);

    /**
     * Run Alice's part of one round: distill the first pair of each half.
     *
     * @param srcQubits Alice's qubits of the pairs.
     * @param dstQubits Bob's qubits of the pairs.
     */
    void DistillateOnce(const std::vector<std::string>& srcQubits,
                        const std::vector<std::string>& dstQubits);

    Ptr<QuantumChannel> m_channel;  //!< The quantum channel from Alice to Bob
    Ptr<QuantumMemory> m_srcQubits; //!< Alice's qubits, one per pair
    Ptr<QuantumMemory> m_dstQubits; //!< Bob's qubits, one per pair
    Time m_interval;                //!< Time between the steps of the protocol
    Time m_occupied;                //!< Time until which the planned steps keep Alice busy
    uint32_t m_nRoundsDone{0};      //!< Number of finished rounds
    bool m_win{true};               //!< Whether no round has failed so far
};

/**
 * @ingroup quantum
 * Destination side (Bob) of nested entanglement distillation.
 *
 * See DistillNestedSrcApp.
 */
class DistillNestedDstApp : public QuantumApplication
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * Get the number of rounds in which Bob has measured.
     *
     * @return The number of finished rounds.
     */
    uint32_t GetNRoundsDone() const;

    /**
     * Get the verdict.
     *
     * @return True if at least one round is over and no round has failed.
     */
    bool GetWin() const;

  private:
    void HandleMessage(const std::string& message, const Address& from) override;

    uint32_t m_nRoundsDone{0}; //!< Number of finished rounds
    bool m_win{true};          //!< Whether no round has failed so far
};

} // namespace ns3

#endif /* DISTILL_NESTED_APP_H */
