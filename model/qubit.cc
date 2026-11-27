/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qubit.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(Qubit);

TypeId
Qubit::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::Qubit").SetParent<Object>().SetGroupName("Quantum").AddConstructor<Qubit>();
    return tid;
}

Qubit::Qubit(const QuantumTensorData& stateVector, const std::string& name)
    : m_stateVector(stateVector),
      m_name(name)
{
}

QuantumTensorData
Qubit::GetStateVector() const
{
    return m_stateVector;
}

std::string
Qubit::GetName() const
{
    return m_name;
}

} // namespace ns3
