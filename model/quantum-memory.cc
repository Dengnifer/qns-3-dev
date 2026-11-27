/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "quantum-memory.h"

#include "ns3/abort.h"
#include "ns3/log.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QuantumMemory");

NS_OBJECT_ENSURE_REGISTERED(QuantumMemory);

TypeId
QuantumMemory::GetTypeId()
{
    static TypeId tid = TypeId("ns3::QuantumMemory")
                            .SetParent<Object>()
                            .SetGroupName("Quantum")
                            .AddConstructor<QuantumMemory>();
    return tid;
}

QuantumMemory::QuantumMemory(const std::vector<std::string>& qubits)
    : m_qubits(qubits)
{
    NS_LOG_FUNCTION(this << qubits.size());
}

void
QuantumMemory::AddQubit(const std::string& qubit)
{
    NS_LOG_FUNCTION(this << qubit);
    m_qubits.push_back(qubit);
}

bool
QuantumMemory::RemoveQubit(const std::string& qubit)
{
    NS_LOG_FUNCTION(this << qubit);
    auto it = std::find(m_qubits.begin(), m_qubits.end(), qubit);
    if (it == m_qubits.end())
    {
        return false;
    }
    m_qubits.erase(it);
    return true;
}

std::size_t
QuantumMemory::GetSize() const
{
    return m_qubits.size();
}

std::string
QuantumMemory::GetQubit(std::size_t index) const
{
    NS_ABORT_MSG_IF(index >= m_qubits.size(),
                    "No qubit at position " << index << " in a memory of size " << m_qubits.size());
    return m_qubits[index];
}

std::vector<std::string>
QuantumMemory::GetQubits() const
{
    return m_qubits;
}

bool
QuantumMemory::HasQubit(const std::string& qubit) const
{
    return std::find(m_qubits.begin(), m_qubits.end(), qubit) != m_qubits.end();
}

} // namespace ns3
