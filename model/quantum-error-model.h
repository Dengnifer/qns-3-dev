/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef QUANTUM_ERROR_MODEL_H
#define QUANTUM_ERROR_MODEL_H

#include "quantum-operation.h"

#include "ns3/nstime.h"
#include "ns3/object.h"

#include <string>

namespace ns3
{

/**
 * @ingroup quantum
 * Base class of the noise models that act on one qubit.
 *
 * A model only describes the noise. QuantumPhyEntity decides when a model
 * applies and to which qubit.
 */
class QuantumErrorModel : public Object
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * Get the noise that the model produces over a duration.
     *
     * @param elapsed Time during which the noise acts on the qubit.
     * @return The noise, as an operation on one qubit.
     */
    virtual QuantumOperation GetOperation(Time elapsed) const = 0;

    /**
     * Describe the model, for logging.
     *
     * @return A one-line description.
     */
    virtual std::string GetDescription() const = 0;
};

/**
 * @ingroup quantum
 * Dephasing of a qubit that waits in a quantum memory.
 *
 * Over a duration t the qubit suffers a Pauli Z error with probability
 * (1 - exp(-t / T)) / 2, where T is the coherence time.
 */
class TimeModel : public QuantumErrorModel
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    QuantumOperation GetOperation(Time elapsed) const override;
    std::string GetDescription() const override;

  private:
    Time m_coherenceTime; //!< Coherence time T
};

/**
 * @ingroup quantum
 * Dephasing of a qubit caused by a gate.
 *
 * The gate is modeled as a wait of fixed duration in a memory with the given
 * coherence time; see TimeModel for the error probability.
 */
class DephaseModel : public QuantumErrorModel
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * @copydoc QuantumErrorModel::GetOperation
     * @note The model ignores @p elapsed and uses its GateDuration attribute.
     */
    QuantumOperation GetOperation(Time elapsed) const override;
    std::string GetDescription() const override;

  private:
    Time m_coherenceTime; //!< Coherence time T
    Time m_gateDuration;  //!< Duration of the gate
};

/**
 * @ingroup quantum
 * Depolarization of a qubit sent over a quantum channel.
 *
 * The qubit is left alone with a probability equal to the fidelity, and
 * otherwise suffers a Pauli X, Y or Z error with equal probabilities. Applied
 * to one half of a Bell pair, the model yields a Werner state of that fidelity.
 */
class DepolarModel : public QuantumErrorModel
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * @copydoc QuantumErrorModel::GetOperation
     * @note The model ignores @p elapsed.
     */
    QuantumOperation GetOperation(Time elapsed) const override;
    std::string GetDescription() const override;

    /**
     * Get the fidelity.
     *
     * @return The probability that the qubit is left alone.
     */
    double GetFidelity() const;

  private:
    double m_fidelity; //!< Probability that the qubit is left alone
};

} // namespace ns3

#endif /* QUANTUM_ERROR_MODEL_H */
