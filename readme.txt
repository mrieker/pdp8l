
PDP-8/L related projects

* automated front-panel replacement
    raspberry-pi based circuit board plugs in front slot
    raspi program runs TCL scripts to load and run tests (maindec and otherwise)

    pipan8l/pcb - circuit board
    pipan8l/pipan8l - raspi program
        provides TCL commands to access light & switch signals

* expansion box replacement
    zynq/zturn based circuit board set (3) plugs in rear slots
    provides functionality of 28KW extended memory, rk05, tc08, ttys, vc8 i & e
    can run with real PDP-8/L or can simulate the PDP-8/L for development

    pipan8l/z8l* - various zynq programs
        with real PDP-8/L only
            z8lreal - set real (non-simulation) mode
        with zynq PDP-8/L simulator only
            z8lsim - set simulation mode
                provides TCL commands to access simulated light & switch signals
            z8lsimtest - test PDP-8/L processor simulation
            z8ltrace - print simulation instruction trace
            z8ltty - access tty 03 line
        runs with either real or simulated PDP-8/L
            z8lcmemtest - test dma access to PDP-8/L core memory
            z8ldump - dump zynq registers
            z8lpin - individual b,c,d 34,35,36 connector pin access
            z8lrk8je - access RK05 disk drives
            z8ltc08 - access TU56 tape drives
            z8ltty - access tty 40,42,44,46 lines
            z8lvc8 - access VC8 I & E
            z8lxmemtest - test extended memory

    zturn/zturn{34,35,36} - circuit boards for rear slots
    zturn/zturnic - interconnects the three boards
    zynq - zynq fpga code
    zplin - zynq petalinux

