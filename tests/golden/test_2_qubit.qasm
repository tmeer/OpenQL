version 1.0
# this file has been automatically generated by the OpenQL compiler please do not modify it manually.
qubits 3

.aKernel
    prepz q[0]
    prepz q[1]
    prepz q[2]
    cz q[0],q[1]
    ry90 q[2]
    x90 q[2]
    measure q[2]
