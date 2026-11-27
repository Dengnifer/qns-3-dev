/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef QUANTUM_MEMORY_H
#define QUANTUM_MEMORY_H

#include "ns3/object.h"

#include <string>
#include <vector>

namespace ns3
{

/**
 * @ingroup quantum
 * An ordered list of qubit names.
 *
 * A QuantumNode keeps the qubits it owns in a memory. Applications also use
 * memories to receive lists of qubits through the attribute system.
 */
class QuantumMemory : public Object
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    QuantumMemory() = default;

    /**
     * Constructor.
     *
     * @param qubits Names of the qubits initially in the memory.
     */
    explicit QuantumMemory(const std::vector<std::string>& qubits);

    /**
     * Append a qubit.
     *
     * @param qubit Name of the qubit.
     */
    void AddQubit(const std::string& qubit);

    /**
     * Remove a qubit.
     *
     * @param qubit Name of the qubit.
     * @return True if the qubit was in the memory.
     */
    bool RemoveQubit(const std::string& qubit);

    /**
     * Get the number of qubits.
     *
     * @return The number of qubits in the memory.
     */
    std::size_t GetSize() const;

    /**
     * Get a qubit by position.
     *
     * @param index Position of the qubit in the memory.
     * @return The name of the qubit.
     */
    std::string GetQubit(std::size_t index) const;

    /**
     * Get all qubits.
     *
     * @return The names of the qubits, in order.
     */
    std::vector<std::string> GetQubits() const;

    /**
     * Check whether a qubit is in the memory.
     *
     * @param qubit Name of the qubit.
     * @return True if the qubit is in the memory.
     */
    bool HasQubit(const std::string& qubit) const;

  private:
    std::vector<std::string> m_qubits; //!< Names of the qubits, in order
};

} // namespace ns3

#endif /* QUANTUM_MEMORY_H */
