version 1.0
# this file has been automatically generated by the OpenQL compiler please do not modify it manually.
qubits 7

.kernel_basic
    measure q[1], b[1]
    wait 14
    cond(b[1]) x q[1]
    { measure q[0], b[0] | measure q[3], b[1] }
    wait 14
    { cond(b[0]) x q[0] | x q[1] | cond(b[1]) x q[3] }
    { measure q[0], b[0] | measure q[1], b[1] }
    wait 3
    measure q[5], b[5]
    { measure q[2], b[2] | measure q[4], b[4] }
    wait 9
    cond(b[0]&&b[1) x q[0]
    cond(!(b[0]&&b[1)) x q[0]
    cond(b[0]||b[1) x q[0]
    cond(!(b[0]||b[1)) x q[0]
    { x q[5] | cond(b[0]^^b[1) x q[0] }
    { cond(b[2]) x q[2] | cond(!b[4]) x q[4] | cond(0) x q[5] | cond(!(b[0]^^b[1)) x q[0] }
