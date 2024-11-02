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

// access the extended memory ram via arm axi bus

module extmemmap (
    input CLOCK,
    input RESET_N,

    // access block memory
    output[14:00] xbraddr,
    output[11:00] xbrwdat,
    input[11:00] xbrrdat,
    output reg xbrenab,
    output reg xbrwena,

    // arm processor memory bus interface (AXI)
    // we are a slave for accessing the control registers (read and write)
    input[16:00]  saxi_ARADDR,
    output reg    saxi_ARREADY,
    input         saxi_ARVALID,
    input[16:00]  saxi_AWADDR,
    output reg    saxi_AWREADY,
    input         saxi_AWVALID,
    input         saxi_BREADY,
    output[1:0]   saxi_BRESP,
    output reg    saxi_BVALID,
    output[31:00] saxi_RDATA,
    input         saxi_RREADY,
    output[1:0]   saxi_RRESP,
    output reg    saxi_RVALID,
    input[31:00]  saxi_WDATA,
    output reg    saxi_WREADY,
    input         saxi_WVALID);

    reg[1:0] reading, writing;
    reg[16:02] readaddr, writeaddr;
    reg[11:00] writedata;

    assign saxi_RDATA = { 20'b0, xbrrdat };                 // return ram data directly to arm
    assign xbrwdat = writedata;                             // send captured write data to ram
    assign xbraddr = (reading != 0) ? readaddr : writeaddr; // send captured address to ram

    // A3.3.1 Read transaction dependencies
    // A3.3.1 Write transaction dependencies
    //        AXI4 write response dependency
    always @(posedge CLOCK) begin
        if (~ RESET_N) begin
            saxi_ARREADY <= 1;                              // we are ready to accept read address
            saxi_RVALID  <= 0;                              // we are not sending out read data

            saxi_AWREADY <= 1;                              // we are ready to accept write address
            saxi_WREADY  <= 1;                              // we are ready to accept write data
            saxi_BVALID  <= 0;                              // we are not acknowledging any write

            reading <= 0;                                   // we don't have a read in progress
            writing <= 0;                                   // we don't have a write in progress
        end else begin

            ///////////////////
            //  read memory  //
            ///////////////////

            // check for PS sending us read address
            if (saxi_ARREADY & saxi_ARVALID) begin
                readaddr <= saxi_ARADDR[16:02];             // save address bits we care about
                saxi_ARREADY <= 0;                          // we are no longer accepting read address
                if (writing == 0) begin
                    reading <= 1;                           // not writing, so start reading
                    xbrenab <= 1;
                    xbrwena <= 0;
                end
            end

            // check for previous read request that was blocked by writing
            else if (~ saxi_ARREADY & (reading == 0) & (writing == 0)) begin
                reading <= 1;
                xbrenab <= 1;
                xbrwena <= 0;
            end

            // check for waiting for read to complete
            else if (reading == 1) begin
                reading <= 2;
            end

            // check for read complete
            else if ((reading == 2) & ~ saxi_RVALID) begin
                saxi_RVALID <= 1;                           // data on saxi_RDATA is now valid
            end

            // check for PS acknowledging receipt of data
            else if (saxi_RVALID & saxi_RREADY) begin
                reading      <= 0;                          // all done with reading
                xbrenab      <= 0;
                saxi_ARREADY <= 1;                          // we are ready to accept an address again
                saxi_RVALID  <= 0;                          // we are no longer sending out data
            end

            ////////////////////
            //  write memory  //
            ////////////////////

            // check for PS sending us write address
            if (saxi_AWREADY & saxi_AWVALID) begin
                writeaddr <= saxi_AWADDR[16:02];            // save address bits we care about
                saxi_AWREADY <= 0;                          // we are no longer accepting write address
                if (~ saxi_WREADY & (reading == 0)) begin
                    writing <= 1;                           // not reading, so start writing
                    xbrenab <= 1;
                    xbrwena <= 1;
                end
            end

            // check for PS sending us write data
            if (saxi_WREADY & saxi_WVALID) begin
                writedata <= saxi_WDATA[11:00];             // save data bits we care about
                saxi_WREADY <= 0;                           // we are no longer accepting write data
                if (~ saxi_AWREADY & (reading == 0)) begin
                    writing <= 1;                           // not reading, so start writing
                    xbrenab <= 1;
                    xbrwena <= 1;
                end
            end

            // see if we have received both write address and write data
            if (~ saxi_AWREADY & ~ saxi_WREADY & ~ saxi_BVALID) begin

                // check for write blocked by previous read
                if ((reading == 0) & (writing == 0)) begin
                    writing <= 1;
                    xbrenab <= 1;
                    xbrwena <= 1;
                end

                // check for write in progress
                else if ((writing > 0) && (writing < 3)) begin
                    writing <= writing + 1;
                end

                // check for end of write
                else if (writing == 3) begin
                    writing <= 0;
                    xbrenab <= 0;
                    xbrwena <= 0;
                    saxi_BVALID <= 1;                       // we have accepted the data
                end
            end

            // check for PS acknowledging write acceptance
            else if (saxi_BVALID & saxi_BREADY) begin
                saxi_BVALID  <= 0;
                saxi_AWREADY <= 1;                          // we are ready to accept write address again
                saxi_WREADY  <= 1;                          // we are ready to accept write data again
            end
        end
    end
endmodule
