version 1.0
# this file has been automatically generated by the OpenQL compiler please do not modify it manually.
qubits 4

.kernel1
    swap q[0],q[1]
    wait 3

.controlled_kernel1
    cnot q[1],q[0]
    wait 3
    { cnot q[2],q[0] | h q[1] }
    wait 1
    t q[1]
    wait 1
    { t q[2] | tdag q[0] }
    wait 1
    cnot q[1],q[0]
    wait 3
    { cnot q[2],q[1] | t q[0] }
    wait 3
    { cnot q[2],q[0] | tdag q[1] }
    wait 3
    { tdag q[0] | cnot q[2],q[1] }
    wait 3
    cnot q[1],q[0]
    wait 3
    { t q[0] | h q[1] }
    wait 1
    cnot q[1],q[0]
    wait 3
