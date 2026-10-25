/*
 * SPDX-License-Identifier: MIT
 * Author: Xue Weisen <xueweisen96@gmail.com>
 * GitHub: https://github.com/DCRimbaud
 *
 * Self-checking AXI4-Lite register testbench.
 * It verifies byte strobes, read-only status, scratch persistence, and the
 * response-channel handshake without requiring Vivado hardware.
 */
`timescale 1ns / 1ps
`default_nettype none

module tb_axi_lite_regs;

    reg         clk;
    reg         resetn;
    reg  [7:0]  awaddr;
    reg  [2:0]  awprot;
    reg         awvalid;
    wire        awready;
    reg  [31:0] wdata;
    reg  [3:0]  wstrb;
    reg         wvalid;
    wire        wready;
    wire [1:0]  bresp;
    wire        bvalid;
    reg         bready;
    reg  [7:0]  araddr;
    reg  [2:0]  arprot;
    reg         arvalid;
    wire        arready;
    wire [31:0] rdata;
    wire [1:0]  rresp;
    wire        rvalid;
    reg         rready;
    reg  [31:0] status_in;
    reg  [31:0] irq_status_in;
    wire [31:0] control_out;
    wire [31:0] scratch0_out;
    wire [31:0] scratch1_out;
    wire [31:0] scratch2_out;
    wire [31:0] scratch3_out;
    wire [31:0] irq_enable_out;

    axi_lite_regs dut (
        .s_axi_aclk(clk),
        .s_axi_aresetn(resetn),
        .s_axi_awaddr(awaddr),
        .s_axi_awprot(awprot),
        .s_axi_awvalid(awvalid),
        .s_axi_awready(awready),
        .s_axi_wdata(wdata),
        .s_axi_wstrb(wstrb),
        .s_axi_wvalid(wvalid),
        .s_axi_wready(wready),
        .s_axi_bresp(bresp),
        .s_axi_bvalid(bvalid),
        .s_axi_bready(bready),
        .s_axi_araddr(araddr),
        .s_axi_arprot(arprot),
        .s_axi_arvalid(arvalid),
        .s_axi_arready(arready),
        .s_axi_rdata(rdata),
        .s_axi_rresp(rresp),
        .s_axi_rvalid(rvalid),
        .s_axi_rready(rready),
        .status_in(status_in),
        .irq_status_in(irq_status_in),
        .control_out(control_out),
        .scratch0_out(scratch0_out),
        .scratch1_out(scratch1_out),
        .scratch2_out(scratch2_out),
        .scratch3_out(scratch3_out),
        .irq_enable_out(irq_enable_out)
    );

    always #5 clk = ~clk;

    task axi_write;
        input [7:0] address;
        input [31:0] data;
        input [3:0] strobe;
        begin
            @(negedge clk);
            awaddr = address;
            awprot = 3'b000;
            awvalid = 1'b1;
            wdata = data;
            wstrb = strobe;
            wvalid = 1'b1;
            while (!(awready && wready)) begin
                @(negedge clk);
            end
            @(posedge clk);
            @(negedge clk);
            awvalid = 1'b0;
            wvalid = 1'b0;
            bready = 1'b1;
            while (!bvalid) begin
                @(negedge clk);
            end
            if (bresp !== 2'b00) begin
                $fatal(1, "unexpected write response");
            end
            @(posedge clk);
            @(negedge clk);
            bready = 1'b0;
        end
    endtask

    task axi_read;
        input [7:0] address;
        input [31:0] expected;
        begin
            @(negedge clk);
            araddr = address;
            arprot = 3'b000;
            arvalid = 1'b1;
            rready = 1'b1;
            while (!arready) begin
                @(negedge clk);
            end
            @(posedge clk);
            @(negedge clk);
            arvalid = 1'b0;
            while (!rvalid) begin
                @(negedge clk);
            end
            if (rresp !== 2'b00) begin
                $fatal(1, "unexpected read response");
            end
            if (rdata !== expected) begin
                $fatal(1, "read mismatch at %02h: got %08h expected %08h", address, rdata, expected);
            end
            @(posedge clk);
            @(negedge clk);
            rready = 1'b0;
        end
    endtask

    initial begin
        clk = 1'b0;
        resetn = 1'b0;
        awaddr = 8'h00;
        awprot = 3'b000;
        awvalid = 1'b0;
        wdata = 32'h0000_0000;
        wstrb = 4'h0;
        wvalid = 1'b0;
        bready = 1'b0;
        araddr = 8'h00;
        arprot = 3'b000;
        arvalid = 1'b0;
        rready = 1'b0;
        status_in = 32'hCAFE_BABE;
        irq_status_in = 32'h0000_0005;

        repeat (3) @(posedge clk);
        @(negedge clk);
        resetn = 1'b1;

        axi_write(8'h00, 32'h1122_3344, 4'hF);
        if (control_out !== 32'h1122_3344) begin
            $fatal(1, "control register output mismatch");
        end

        axi_write(8'h08, 32'hAAAA_AAAA, 4'hF);
        axi_write(8'h08, 32'h0000_0055, 4'h1);
        if (scratch0_out !== 32'hAAAA_AA55) begin
            $fatal(1, "byte strobe mismatch");
        end

        axi_read(8'h04, 32'hCAFE_BABE);
        axi_read(8'h00, 32'h1122_3344);
        axi_read(8'h08, 32'hAAAA_AA55);
        axi_read(8'h1C, 32'h0000_0005);
        axi_read(8'h20, 32'h0000_0000);

        axi_write(8'h18, 32'h0000_0003, 4'hF);
        if (irq_enable_out !== 32'h0000_0003) begin
            $fatal(1, "IRQ enable output mismatch");
        end

        $display("AXI-Lite register test: PASS");
        $finish;
    end

    initial begin
        #10000;
        $fatal(1, "testbench timeout");
    end

endmodule

`default_nettype wire
