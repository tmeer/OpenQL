from openql import openql as ql
import os

_curdir_ = os.path.dirname(__file__)

#-------------------------------------------------------------------------------

def first_example():
    output_dir = os.path.join(_curdir_, 'test_output')
    ql.set_option('output_dir', output_dir)

    # You can specify a config location, here we use a default config
    config_fn = os.path.join(_curdir_, 'spin_demo_2811.json')
    platform = ql.Platform("spin_qubit_demo", config_fn)
    sweep_points = [1, 2]
    num_qubits = 2
    p = ql.Program("program", platform, num_qubits)
    p.set_sweep_points(sweep_points, len(sweep_points))

    # populate kernel using default gates
    k = ql.Kernel("kernel", platform, num_qubits)
    #k.prepz(0)

#####

    k.rx90(0)  # 145ns,ch 4,cw 1
    k.ry90(0)  # 145ns,ch 4,cw 3
    k.mrx90(0) # 145ns,ch 4,cw 2
    k.mry90(0) # 145ns,ch 4,cw 4
    k.identity(0) # 125ns,ch 4,cw 5

    #k.mry90(1) # 145ns,ch 5,cw 4
    k.ry90(1) # 145ns,ch 5,cw 3

#####

    # add the kernel to the program
    p.add_kernel(k)

    # compile the program
    p.compile()

#-------------------------------------------------------------------------------

if __name__ == '__main__':
    first_example()


#-------------------------------------------------------------------------------


#wait 1
#mov r12, 1
#mov r13, 0
#start:
#wait 2
#trigger 1000000, 1
#wait 2
#trigger 1000000, 1
#wait 1
#beq r13, r13 start
