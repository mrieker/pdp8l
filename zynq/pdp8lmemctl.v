//    Copyright (C) Mike Rieker, Beverly, MA USA
//    www.outerworldapps.com
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; version 2 of the License.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    EXPECT it to FAIL when someone's HeALTh or PROpeRTy is at RISk.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//    http://www.gnu.org/licenses/gpl-2.0.html

// PDP-8/L memory controller
// generates timing pulses like real PDP-8/L
// follows timing from vol 1, p 4-14

module pdp8lmemctl (
    input CLOCK, CSTEP, RESET,

    input memstart,
    input select,

    // all timing relative to rising edge of memstart
    // all signals zero if not selected
    // memenab guaranteed to be off at least one FPGA clock at end of mem cycle
    output reg memenab,     // 0.01..1.60 uS
    output reg read,        // 0.25..0.75 uS    read in progress
    output reg strobe,      // 0.50..0.60 uS    read complete
    output reg lock,        // 0.25..1.00 uS
    output reg cycdone,     // 0.75..0.85 uS
    output reg cycle,       // 0.75..1.15 uS
    output reg inhibit,     // 0.95..1.50 uS
    output reg write,       // 1.00..1.50 uS    write in progress
    output reg memdone      // 1.50..1.60 uS    write complete
);

    reg[7:0] memdelay;

    always @(posedge CLOCK) begin
        if (RESET) begin
            memdelay <= 0;
            memenab  <= 0;
            read     <= 0;
            strobe   <= 0;
            lock     <= 0;
            cycdone  <= 0;
            cycle    <= 0;
            inhibit  <= 0;
            write    <= 0;
            memdone  <= 0;
        end else if (CSTEP) begin
            if (memdelay == 0) begin
                if (memstart & select) begin
                    memdelay <= 1;
                    memenab  <= 1;
                end
            end else if (memdelay != 160) begin
                memdelay <= memdelay + 1;
            end else begin
                memdelay <= 0;
            end

            case (memdelay)
                 25: begin
                    read    <= 1;
                    lock    <= 1;
                end
                 50: begin
                    strobe  <= 1;
                end
                 60: begin
                    strobe  <= 0;
                end
                 75: begin
                    read    <= 0;
                    cycdone <= 1;
                    cycle   <= 1;
                end
                 85: begin
                    cycdone <= 0;
                end
                 95: begin
                    inhibit <= 1;
                end
                100: begin
                    lock    <= 0;
                    write   <= 1;
                end
                115: begin
                    cycle   <= 0;
                end
                150: begin
                    inhibit <= 0;
                    write   <= 0;
                    memdone <= 1;
                end
                160: begin
                    memdone <= 0;
                    memenab <= 0;
                end
            endcase
        end
    end
endmodule
