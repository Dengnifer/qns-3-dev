/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "quantum-operation.h"

#include "ns3/abort.h"

#include <cmath>
#include <numeric>

namespace ns3
{

QuantumOperation::QuantumOperation(const std::vector<std::string>& names,
                                   const std::vector<QuantumTensorData>& gates,
                                   const std::vector<double>& probabilities)
    : m_names(names),
      m_probabilities(probabilities)
{
    NS_ABORT_MSG_IF(gates.empty(), "A quantum operation needs at least one gate");
    NS_ABORT_MSG_IF(names.size() != gates.size() || probabilities.size() != gates.size(),
                    "A quantum operation needs one name and one probability per gate");
    const double total = std::accumulate(probabilities.begin(), probabilities.end(), 0.0);
    NS_ABORT_MSG_IF(std::abs(total - 1.0) > QNS_EPSILON,
                    "The probabilities of a quantum operation must sum to 1, got " << total);

    m_kraus.reserve(gates.size());
    for (std::size_t i = 0; i < gates.size(); ++i)
    {
        NS_ABORT_MSG_IF(gates[i].size() != gates[0].size(),
                        "The gates of a quantum operation must have the same size");
        NS_ABORT_MSG_IF(probabilities[i] < 0.0, "Negative probability " << probabilities[i]);
        const double scale = std::sqrt(probabilities[i]);
        QuantumTensorData kraus = gates[i];
        for (auto& entry : kraus)
        {
            entry *= scale;
        }
        m_kraus.push_back(std::move(kraus));
    }
    // A gate on n qubits has 2^n * 2^n entries.
    NS_ABORT_MSG_IF(CountQubits(gates[0].size()) % 2 != 0, "A gate must be a square matrix");
}

std::string
QuantumOperation::GetName(std::size_t index) const
{
    return m_names.at(index);
}

double
QuantumOperation::GetProbability(std::size_t index) const
{
    return m_probabilities.at(index);
}

const std::vector<QuantumTensorData>&
QuantumOperation::GetKrausOperators() const
{
    return m_kraus;
}

std::size_t
QuantumOperation::GetSize() const
{
    return m_kraus.size();
}

unsigned
QuantumOperation::GetNQubits() const
{
    return CountQubits(m_kraus[0].size()) / 2;
}

} // namespace ns3
