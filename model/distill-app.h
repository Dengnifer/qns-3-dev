/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef DISTILL_APP_H
#define DISTILL_APP_H

#include "quantum-application.h"

#include "ns3/nstime.h"

#include <string>
#include <utility>

namespace ns3
{

class QuantumChannel;

/**
 * @ingroup quantum
 * Source side (Alice) of one round of entanglement distillation.
 *
 * Alice and Bob share two Bell pairs, the goal pair and the pair to measure.
 * Each applies a CNOT from the goal qubit to the other qubit and measures the
 * latter. The round succeeds if the two outcomes agree, and then leaves the
 * goal pair with a higher fidelity.
 *
 * Alice distributes both pairs, measures first and sends her outcome to Bob,
 * who replies with the verdict.
 */
class DistillSrcApp : public QuantumApplication
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * Check whether Bob has sent the verdict.
     *
     * @return True once the round is over.
     */
    bool IsDone() const;

    /**
     * Get the verdict.
     *
     * @return True if the round is over and succeeded.
     */
    bool GetWin() const;

  protected:
    void DoDispose() override;

  private:
    void DoStartApplication() override;
    void HandleMessage(const std::string& message, const Address& from) override;

    /**
     * Apply Alice's CNOT, measure, and send the outcome to Bob.
     */
    void MeasureAndSend();

    Ptr<QuantumChannel> m_channel;                 //!< The quantum channel from Alice to Bob
    std::pair<std::string, std::string> m_eprGoal; //!< The pair to keep: Alice's and Bob's qubit
    std::pair<std::string, std::string> m_eprMeas; //!< The pair to measure: Alice's and Bob's qubit
    Time m_interval;                               //!< Time between the steps of the protocol
    bool m_done{false};                            //!< Whether the verdict has arrived
    bool m_win{false};                             //!< The verdict
};

/**
 * @ingroup quantum
 * Destination side (Bob) of one round of entanglement distillation.
 *
 * See DistillSrcApp.
 */
class DistillDstApp : public QuantumApplication
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * Check whether Bob has measured.
     *
     * @return True once the round is over.
     */
    bool IsDone() const;

    /**
     * Get the verdict.
     *
     * @return True if the round is over and succeeded.
     */
    bool GetWin() const;

  private:
    void HandleMessage(const std::string& message, const Address& from) override;

    std::pair<std::string, std::string>
        m_qubits;       //!< Bob's qubits of the goal pair and of the pair to measure
    bool m_done{false}; //!< Whether Bob has measured
    bool m_win{false};  //!< The verdict
};

} // namespace ns3

#endif /* DISTILL_APP_H */
