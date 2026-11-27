/*
 * Copyright (c) 2024 The qns-3 authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "quantum-basis.h"

#include "ns3/abort.h"

#include <map>

namespace ns3
{

const QuantumTensorData*
FindBuiltinGate(const std::string& name)
{
    static const std::map<std::string, const QuantumTensorData*> builtinGates{
        {QNS_GATE_I, &QNS_PAULI_I},
        {QNS_GATE_PX, &QNS_PAULI_X},
        {QNS_GATE_PY, &QNS_PAULI_Y},
        {QNS_GATE_PZ, &QNS_PAULI_Z},
        {QNS_GATE_H, &QNS_HADAMARD},
        {QNS_GATE_CNOT, &QNS_CNOT},
        {QNS_GATE_CZ, &QNS_CZ},
        {QNS_GATE_SWAP, &QNS_SWAP},
        {QNS_GATE_TOFF, &QNS_TOFFOLI},
        {QNS_GATE_QOR, &QNS_QOR},
    };
    auto it = builtinGates.find(name);
    return it == builtinGates.end() ? nullptr : it->second;
}

QuantumTensorData
MakeWernerState(double fidelity)
{
    NS_ABORT_MSG_IF(fidelity < 0.0 || fidelity > 1.0,
                    "A fidelity must lie in [0, 1], got " << fidelity);
    const double diagonal = (2 * fidelity + 1) / 6;
    const double offDiagonal = (4 * fidelity - 1) / 6;
    const double mismatch = (1 - fidelity) / 3;
    return {
        {diagonal, 0.0},
        {0.0, 0.0},
        {0.0, 0.0},
        {offDiagonal, 0.0},
        {0.0, 0.0},
        {mismatch, 0.0},
        {0.0, 0.0},
        {0.0, 0.0},
        {0.0, 0.0},
        {0.0, 0.0},
        {mismatch, 0.0},
        {0.0, 0.0},
        {offDiagonal, 0.0},
        {0.0, 0.0},
        {0.0, 0.0},
        {diagonal, 0.0},
    };
}

unsigned
CountQubits(std::size_t size)
{
    NS_ABORT_MSG_IF(size == 0 || (size & (size - 1)) != 0,
                    "The size of quantum data must be a power of two, got " << size);
    unsigned count = 0;
    while (size > 1)
    {
        size >>= 1;
        ++count;
    }
    return count;
}

std::vector<std::string>
GetFirstHalf(const std::vector<std::string>& qubits)
{
    return {qubits.begin(), qubits.begin() + qubits.size() / 2};
}

std::vector<std::string>
GetSecondHalf(const std::vector<std::string>& qubits)
{
    return {qubits.begin() + qubits.size() / 2, qubits.end()};
}

} // namespace ns3
