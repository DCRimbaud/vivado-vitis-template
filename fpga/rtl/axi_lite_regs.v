/*
 * SPDX-License-Identifier: MIT
 * Author: Xue Weisen <xueweisen96@gmail.com>
 * GitHub: https://github.com/DCRimbaud
 *
 * Generic AXI4-Lite register block for platform bring-up.
 *
 * Register map:
 *   0x00 CONTROL       R/W
 *   0x04 STATUS        RO
 *   0x08 SCRATCH0      R/W
 *   0x0C SCRATCH1      R/W
 *   0x10 SCRATCH2      R/W
 *   0x14 SCRATCH3      R/W
 *   0x18 IRQ_ENABLE    R/W
 *   0x1C IRQ_STATUS    RO
 *
 * The block accepts one outstanding write and one outstanding read. AW and W
 * may arrive in different cycles; the write commits only after both channels
 * have been captured. Unknown addresses return zero on read and OKAY on write.
 */
`timescale 1ns / 1ps
`default_nettype none

module axi_lite_regs #(
    parameter integer ADDR_WIDTH = 8
) (
    input  wire                    s_axi_aclk,
    input  wire                    s_axi_aresetn,

    input  wire [ADDR_WIDTH-1:0]   s_axi_awaddr,
    input  wire [2:0]              s_axi_awprot,
    input  wire                    s_axi_awvalid,
    output wire                    s_axi_awready,

    input  wire [31:0]             s_axi_wdata,
    input  wire [3:0]              s_axi_wstrb,
    input  wire                    s_axi_wvalid,
    output wire                    s_axi_wready,

    output wire [1:0]              s_axi_bresp,
    output reg                     s_axi_bvalid,
    input  wire                    s_axi_bready,

    input  wire [ADDR_WIDTH-1:0]   s_axi_araddr,
    input  wire [2:0]              s_axi_arprot,
    input  wire                    s_axi_arvalid,
    output wire                    s_axi_arready,

    output reg  [31:0]             s_axi_rdata,
    output wire [1:0]              s_axi_rresp,
    output reg                     s_axi_rvalid,
    input  wire                    s_axi_rready,

    input  wire [31:0]             status_in,
    input  wire [31:0]             irq_status_in,
    output wire [31:0]             control_out,
    output wire [31:0]             scratch0_out,
    output wire [31:0]             scratch1_out,
    output wire [31:0]             scratch2_out,
    output wire [31:0]             scratch3_out,
    output wire [31:0]             irq_enable_out
);

    localparam [1:0] RESP_OKAY = 2'b00;

    localparam [31:0] REG_CONTROL    = 32'h0000_0000;
    localparam [31:0] REG_STATUS     = 32'h0000_0004;
    localparam [31:0] REG_SCRATCH0   = 32'h0000_0008;
    localparam [31:0] REG_SCRATCH1   = 32'h0000_000C;
    localparam [31:0] REG_SCRATCH2   = 32'h0000_0010;
    localparam [31:0] REG_SCRATCH3   = 32'h0000_0014;
    localparam [31:0] REG_IRQ_ENABLE = 32'h0000_0018;
    localparam [31:0] REG_IRQ_STATUS = 32'h0000_001C;

    reg [31:0] control_reg;
    reg [31:0] scratch0_reg;
    reg [31:0] scratch1_reg;
    reg [31:0] scratch2_reg;
    reg [31:0] scratch3_reg;
    reg [31:0] irq_enable_reg;

    reg [ADDR_WIDTH-1:0] awaddr_latched;
    reg [31:0]           wdata_latched;
    reg [3:0]            wstrb_latched;
    reg                  aw_pending;
    reg                  w_pending;

    wire [31:0] awaddr_ext = {{(32-ADDR_WIDTH){1'b0}}, s_axi_awaddr};
    wire [31:0] araddr_ext = {{(32-ADDR_WIDTH){1'b0}}, s_axi_araddr};
    wire [31:0] awaddr_latched_ext = {{(32-ADDR_WIDTH){1'b0}}, awaddr_latched};
    wire        write_commit = aw_pending && w_pending;

    assign s_axi_awready = (~aw_pending) && (~s_axi_bvalid);
    assign s_axi_wready  = (~w_pending) && (~s_axi_bvalid);
    assign s_axi_arready = ~s_axi_rvalid;
    assign s_axi_bresp   = RESP_OKAY;
    assign s_axi_rresp   = RESP_OKAY;

    assign control_out     = control_reg;
    assign scratch0_out    = scratch0_reg;
    assign scratch1_out    = scratch1_reg;
    assign scratch2_out    = scratch2_reg;
    assign scratch3_out    = scratch3_reg;
    assign irq_enable_out  = irq_enable_reg;

    /* Apply byte strobes without changing bytes that are not selected. */
    function [31:0] apply_wstrb;
        input [31:0] current_value;
        input [31:0] new_value;
        input [3:0]  strobe;
        begin
            apply_wstrb[7:0]   = strobe[0] ? new_value[7:0]   : current_value[7:0];
            apply_wstrb[15:8]  = strobe[1] ? new_value[15:8]  : current_value[15:8];
            apply_wstrb[23:16] = strobe[2] ? new_value[23:16] : current_value[23:16];
            apply_wstrb[31:24] = strobe[3] ? new_value[31:24] : current_value[31:24];
        end
    endfunction

    /* Read mux is combinational and returns zero for unmapped offsets. */
    function [31:0] read_mux;
        input [31:0] address;
        begin
            case (address[7:0])
                REG_CONTROL:    read_mux = control_reg;
                REG_STATUS:     read_mux = status_in;
                REG_SCRATCH0:   read_mux = scratch0_reg;
                REG_SCRATCH1:   read_mux = scratch1_reg;
                REG_SCRATCH2:   read_mux = scratch2_reg;
                REG_SCRATCH3:   read_mux = scratch3_reg;
                REG_IRQ_ENABLE: read_mux = irq_enable_reg;
                REG_IRQ_STATUS: read_mux = irq_status_in;
                default:        read_mux = 32'h0000_0000;
            endcase
        end
    endfunction

    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            control_reg    <= 32'h0000_0000;
            scratch0_reg   <= 32'h0000_0000;
            scratch1_reg   <= 32'h0000_0000;
            scratch2_reg   <= 32'h0000_0000;
            scratch3_reg   <= 32'h0000_0000;
            irq_enable_reg <= 32'h0000_0000;

            awaddr_latched <= {ADDR_WIDTH{1'b0}};
            wdata_latched  <= 32'h0000_0000;
            wstrb_latched  <= 4'h0;
            aw_pending     <= 1'b0;
            w_pending      <= 1'b0;

            s_axi_bvalid   <= 1'b0;
            s_axi_rvalid   <= 1'b0;
            s_axi_rdata    <= 32'h0000_0000;
        end else begin
            /* A completed response releases the channel for the next request. */
            if (s_axi_bvalid && s_axi_bready) begin
                s_axi_bvalid <= 1'b0;
            end
            if (s_axi_rvalid && s_axi_rready) begin
                s_axi_rvalid <= 1'b0;
            end

            if (write_commit) begin
                case (awaddr_latched_ext[7:0])
                    REG_CONTROL:    control_reg    <= apply_wstrb(control_reg, wdata_latched, wstrb_latched);
                    REG_SCRATCH0:   scratch0_reg   <= apply_wstrb(scratch0_reg, wdata_latched, wstrb_latched);
                    REG_SCRATCH1:   scratch1_reg   <= apply_wstrb(scratch1_reg, wdata_latched, wstrb_latched);
                    REG_SCRATCH2:   scratch2_reg   <= apply_wstrb(scratch2_reg, wdata_latched, wstrb_latched);
                    REG_SCRATCH3:   scratch3_reg   <= apply_wstrb(scratch3_reg, wdata_latched, wstrb_latched);
                    REG_IRQ_ENABLE: irq_enable_reg <= apply_wstrb(irq_enable_reg, wdata_latched, wstrb_latched);
                    default: begin
                        /* Read-only and unknown addresses still complete with OKAY. */
                    end
                endcase
                aw_pending   <= 1'b0;
                w_pending    <= 1'b0;
                s_axi_bvalid <= 1'b1;
            end else begin
                if (s_axi_awvalid && s_axi_awready) begin
                    awaddr_latched <= s_axi_awaddr;
                    aw_pending     <= 1'b1;
                end
                if (s_axi_wvalid && s_axi_wready) begin
                    wdata_latched <= s_axi_wdata;
                    wstrb_latched <= s_axi_wstrb;
                    w_pending     <= 1'b1;
                end
            end

            if (s_axi_arvalid && s_axi_arready) begin
                s_axi_rdata  <= read_mux(araddr_ext);
                s_axi_rvalid <= 1'b1;
            end
        end
    end

endmodule

`default_nettype wire
