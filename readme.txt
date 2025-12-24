
PDP-8/L related projects

* automated front-panel replacement
    raspberry-pi based circuit board plugs in front slot
    raspi program runs TCL scripts to load and run tests (maindec and otherwise)

    pipan8l/pcb - circuit board
    pipan8l/pipan8l - raspi program
        provides TCL commands to access light & switch signals

* automated front-panel overlay
    I2C based circuit board piggy-backs onto row 1 of backplane with real panel plugged into slot 1
    connects to zturn board plugged into row 36 via 3-wire I2C cable
    zturn program runs TCL scripts to load and run tests (maindec and otherwise)

    pipan8l/pcb3 - circuit board (through hole parts)
    pipan8l/pcb4 - circuit board (surface mount parts)
    pipan8l/z8lpanel - zturn program
        provides TCL commands to access light & switch signals
            senses lights and switches
            can also override switches for automated testing and booting

* expansion box replacement
    zynq/zturn-7020 based circuit board set (4) plugs in rear slots
    provides functionality of 28KW extended memory, rk05, tc08, ttys, vc8 i & e
    can also provide base 4KW memory or use real PDP-8/L 4KW core
    can run with real PDP-8/L or can simulate the PDP-8/L for development

    pipan8l/z8l* - various zynq programs
        with real PDP-8/L only
            z8lreal - set fpga to real (non-simulation) mode
            z8lpanel - access front panel overlay board
                provides TCL commands to access real pdp light & switch signals
                can override or just sense real switches
        with zynq PDP-8/L simulator only
            can run on zturn board by itself or with all boards plugged into real pdp
            z8lsim - set fpga to simulation mode
                provides TCL commands to access simulated light & switch signals
            z8ltrace - print simulation instruction trace
        runs with either real or simulated PDP-8/L
            z8lcmemtest - test dma access to PDP-8/L core and zynq fpga memories
            z8ldump - dump zynq registers
            z8lmctrace - print memory cycle trace
            z8lrk8je - access RK05 disk drives
            z8lsimtest - test PDP-8/L processor with random instructions and data
            z8ltc08 - access TU56 tape drives
            z8ltty - access tty 03,40,42,44,46 lines
            z8lvc8 - access VC8 I & E
            z8lxmemtest - test extended memory

    zturn/zturn{34b,34d,35,36} - circuit boards for rear slots
    zturn/zturnic - interconnects the three boards (3 needed)
    zynq - zynq fpga code
    zplin - zynq petalinux

    see quickstart.txt for example programs

