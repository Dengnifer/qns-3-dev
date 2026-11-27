/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef QUBIT_H
#define QUBIT_H

#include "quantum-basis.h"

#include "ns3/object.h"

#include <string>

namespace ns3
{

/**
 * @ingroup quantum
 * A named qubit together with the pure state it is to be prepared in.
 *
 * Applications receive the qubits they must prepare as Qubit objects through
 * the attribute system. The live state of a qubit is held by QuantumPhyEntity.
 */
class Qubit : public Object
{
  public:
    /**
     * Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    Qubit() = default;

    /**
     * Constructor.
     *
     * @param stateVector State vector to prepare; empty if the qubit already exists.
     * @param name Name of the qubit.
     */
    Qubit(const QuantumTensorData& stateVector, const std::string& name);

    /**
     * Get the state to prepare.
     *
     * @return The state vector; empty if the qubit already exists.
     */
    QuantumTensorData GetStateVector() const;

    /**
     * Get the name.
     *
     * @return The name of the qubit.
     */
    std::string GetName() const;

  private:
    QuantumTensorData m_stateVector; //!< State vector to prepare
    std::string m_name;              //!< Name of the qubit
};

} // namespace ns3

#endif /* QUBIT_H */
