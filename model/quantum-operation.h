/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef QUANTUM_OPERATION_H
#define QUANTUM_OPERATION_H

#include "quantum-basis.h"

#include <string>
#include <vector>

namespace ns3
{

/**
 * @ingroup quantum
 * A quantum operation given as a probabilistic mixture of gates.
 *
 * The operation maps a state rho to the sum over i of p_i U_i rho U_i^dagger,
 * where U_i is the i-th gate and p_i its probability. The class stores the
 * Kraus operators sqrt(p_i) U_i.
 */
class QuantumOperation
{
  public:
    /**
     * Constructor.
     *
     * @param names Names of the gates, for logging.
     * @param gates Matrices of the gates, all of the same size.
     * @param probabilities Probability of each gate; they must sum to one.
     */
    QuantumOperation(const std::vector<std::string>& names,
                     const std::vector<QuantumTensorData>& gates,
                     const std::vector<double>& probabilities);

    /**
     * Get the name of a gate.
     *
     * @param index Index of the gate.
     * @return The name of the gate.
     */
    std::string GetName(std::size_t index) const;

    /**
     * Get the probability of a gate.
     *
     * @param index Index of the gate.
     * @return The probability of the gate.
     */
    double GetProbability(std::size_t index) const;

    /**
     * Get the Kraus operators.
     *
     * @return The gates, each scaled by the square root of its probability.
     */
    const std::vector<QuantumTensorData>& GetKrausOperators() const;

    /**
     * Get the number of gates in the mixture.
     *
     * @return The number of gates.
     */
    std::size_t GetSize() const;

    /**
     * Get the number of qubits the operation acts on.
     *
     * @return The number of qubits.
     */
    unsigned GetNQubits() const;

  private:
    std::vector<std::string> m_names;       //!< Names of the gates
    std::vector<QuantumTensorData> m_kraus; //!< Kraus operators
    std::vector<double> m_probabilities;    //!< Probabilities of the gates
};

} // namespace ns3

#endif /* QUANTUM_OPERATION_H */
