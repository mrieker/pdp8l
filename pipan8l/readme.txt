
RasPI-Zero-W uses /dev/i2c-1 to access the I2C bus going to the PCB.
The PCB plugs into slot 1 of the PDP-8/L in place of the console.

    RasPI-Zero-W ./pipanel  ==>  PCB MCP23017s  ==>  PDP-8/L slot 1  ==>  rest of PDP-8/L

Can also be used with ZTurn Zynq for testing pipanel using the pdp8v submodule.

Method 1:

    Run both raspictl (PDP-8/V simulator) and pipanel code on the Zynq
    It uses an internal Zynq I2C and requires no wiring
    Run raspictl in one terminal window on Zynq
    Set envar i2clibdev=zynq before running pipanel script in another window

    ZTurn ./pipanel  ==>  ZTurn (Zynq emulating I2CMaster, MCP23017s and PDP-8/V)

Method 2:

    Requires wiring the I2C between the RasPI and the Zynq:
        Any RasPI ground to any Zynq ground
        RasPI SCL pin 3 to Zynq pin H20 (ZTurn CN2-65)
        RasPI SDA pin 2 to Zynq pin G15 (ZTurn CN2-69)
        - the Zynq pins can be changed in the ledpins.xdc file

    Run raspictl (PDP-8/V simulator) on the Zynq
        - Zynq looks like 5 MCP23017s on the I2C bus,
          just like the PCB that plugs into the PDP-8/L

    Run pipanel on the RasPI

    RasPI-Zero-W ./pipanel  ==>  ZTurn (Zynq emulating MCP23017s and PDP-8/V)

