/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef QUANTUM_NETWORK_SIMULATOR_H
#define QUANTUM_NETWORK_SIMULATOR_H

#include "quantum-basis.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ns3
{

class QuantumOperation;

/**
 * @ingroup quantum
 * Result of measuring one qubit in the computational basis.
 */
struct QuantumMeasurement
{
    unsigned outcome{0};              //!< The outcome, 0 or 1
    std::vector<double> distribution; //!< Probabilities of the outcomes 0 and 1
};

/**
 * @ingroup quantum
 * Tensor-network engine that holds the joint state of all qubits.
 *
 * The engine keeps the density matrix of the network as a tensor network and
 * appends one tensor per state preparation, gate, noise operation or partial
 * trace. Nothing is contracted until a number is needed: a measurement
 * probability, a density matrix or a fidelity.
 *
 * The engine knows nothing about nodes, ownership, time or noise models; see
 * QuantumPhyEntity for those. The owner names it takes are used for logging
 * only.
 *
 * The tensor contractions are delegated to the ExaTN library, which is an
 * implementation detail hidden behind this interface.
 */
class QuantumNetworkSimulator
{
  public:
    QuantumNetworkSimulator();
    ~QuantumNetworkSimulator();

    // The engine owns tensors registered with ExaTN under unique names.
    QuantumNetworkSimulator(const QuantumNetworkSimulator&) = delete;
    QuantumNetworkSimulator& operator=(const QuantumNetworkSimulator&) = delete;

    /**
     * Create qubits in a pure state.
     *
     * @param owner Owner that creates the qubits.
     * @param data State vector with 2^n entries, for n qubits.
     * @param qubits Unique names of the n qubits.
     */
    void GenerateQubitsPure(const std::string& owner,
                            const QuantumTensorData& data,
                            const std::vector<std::string>& qubits);

    /**
     * Create qubits in a mixed state.
     *
     * @param owner Owner that creates the qubits.
     * @param data Density matrix with 2^n * 2^n entries, for n qubits.
     * @param qubits Unique names of the n qubits.
     */
    void GenerateQubitsMixed(const std::string& owner,
                             const QuantumTensorData& data,
                             const std::vector<std::string>& qubits);

    /**
     * Apply a gate.
     *
     * @param owner Owner that applies the gate.
     * @param gate Name of the gate.
     * @param data Matrix of the gate with 2^n * 2^n entries. Ignored for a
     *             built-in gate; may be empty for a gate applied before.
     * @param qubits Names of the n qubits the gate acts on.
     */
    void ApplyGate(const std::string& owner,
                   const std::string& gate,
                   const QuantumTensorData& data,
                   const std::vector<std::string>& qubits);

    /**
     * Apply a quantum operation.
     *
     * @param operation The operation.
     * @param qubits Names of the qubits the operation acts on.
     */
    void ApplyOperation(const QuantumOperation& operation, const std::vector<std::string>& qubits);

    /**
     * Measure one qubit in the computational basis and project the state.
     *
     * @param owner Owner that measures.
     * @param qubit Name of the qubit.
     * @param sample A sample drawn uniformly from [0, 1); the outcome is 0 if
     *               the sample is below the probability of outcome 0.
     * @return The outcome and the probabilities of both outcomes.
     */
    QuantumMeasurement Measure(const std::string& owner, const std::string& qubit, double sample);

    /**
     * Project one qubit onto a chosen measurement outcome and renormalize.
     *
     * This conditions the state on the outcome without sampling, which is how
     * an exact simulation reads a flag qubit.
     *
     * @param owner Owner that post-selects.
     * @param qubit Name of the qubit.
     * @param outcome The outcome to keep, 0 or 1; it must have a positive probability.
     * @return The probability that a measurement would have given @p outcome.
     */
    double PostSelect(const std::string& owner, const std::string& qubit, unsigned outcome);

    /**
     * Compute the reduced density matrix of some qubits without disturbing them.
     *
     * @param owner Owner that asks.
     * @param qubits Names of the n qubits.
     * @return The density matrix, with 2^n * 2^n entries.
     */
    QuantumTensorData PeekDensityMatrix(const std::string& owner,
                                        const std::vector<std::string>& qubits);

    /**
     * Discard qubits.
     *
     * @param qubits Names of the qubits to trace out.
     */
    void PartialTrace(const std::vector<std::string>& qubits);

    /**
     * Contract the tensor network into a single tensor.
     *
     * Contracting discards the tensors of the traced-out qubits, which keeps
     * the cost of later queries low.
     *
     * @param optimizer Name of the contraction order optimizer. "auto" builds
     *                  a few orders and keeps the cheapest; use it unless you
     *                  have a reason not to. "ascend" contracts the tensors in
     *                  order of creation. "dummy", "heuro", "greed", "metis"
     *                  and "cutnn" are the searches of ExaTN, which take
     *                  minutes on networks of a thousand tensors.
     * @return The density matrix of all remaining qubits.
     */
    QuantumTensorData Contract(const std::string& optimizer);

    /**
     * Compute the fidelity of two qubits with respect to the Bell state #QNS_BELL.
     *
     * @param epr Names of the two qubits.
     * @return The fidelity.
     */
    double CalculateFidelity(const std::pair<std::string, std::string>& epr);

    /**
     * Check whether qubits exist and have not been traced out.
     *
     * @param qubits Names of the qubits.
     * @return True if all of the qubits are valid.
     */
    bool AreValid(const std::vector<std::string>& qubits) const;

    /**
     * Get the qubits that exist and have not been traced out.
     *
     * @return The names of the valid qubits, in order of creation.
     */
    std::vector<std::string> GetValidQubits() const;

    /**
     * Get the size of the tensor network.
     *
     * @return The number of tensors in the network.
     */
    std::size_t GetNTensors() const;

  private:
    class Impl;
    std::unique_ptr<Impl> m_impl; //!< ExaTN-specific state
};

} // namespace ns3

#endif /* QUANTUM_NETWORK_SIMULATOR_H */
