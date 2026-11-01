# Task E: Embedded-system integration design

Block diagram: `src/web/block_diagram.svg` (also shown in the Hardware view). Mock adapters:
`src/core/hardware.{h,cpp}`. Everything below is a **design**; in this prototype all device behaviour is
**simulated** and no hardware is touched.

## Devices (assumed models - each is a design choice, not a requirement)

| Role | Assumed board | Why this kind of board |
|---|---|---|
| Microcontroller | ST Nucleo-F446RE (STM32F446RE, Cortex-M4) | On-board ST-LINK gives USB power + virtual COM port; JTAG/SWD pins accessible on Morpho headers. |
| FPGA | Digilent Arty A7-35T (Xilinx XC7A35T) | On-board FT2232H provides both JTAG configuration and a USB-UART over one cable. |
| JTAG debug adapter | SEGGER J-Link EDU Mini | Small USB debug probe for Arm targets; educational licence. |

## Interface and pin-voltage table

| Link | Connector / signals | Voltage domain | Level shifting | Owner / notes |
|---|---|---|---|---|
| Host ↔ powered hub | USB 2.0 Type-A | 5 V VBUS, USB differential | n/a | Hub has its own PSU so three boards do not overload a laptop port. |
| Hub ↔ Nucleo | USB Mini-B to ST-LINK/V2-1 | 5 V in → on-board 3.3 V regulator | n/a | Virtual COM port (USART2), 115200 8N1 telemetry. |
| Hub ↔ Arty A7 | USB Micro-B to FT2232H | 5 V in → on-board regulators: 3.3 V I/O, 1.0 V VCCINT | n/a | FT2232H channel A = FPGA JTAG (configuration), channel B = UART. 7-15 V barrel jack if a design draws more than USB allows. |
| Hub ↔ J-Link EDU Mini | USB Micro-B | 5 V bus-powered probe | n/a | Host talks to it through SEGGER's J-Link software (JLinkARM.dll). |
| J-Link → Nucleo MCU | 10-pin 1.27 mm Cortex Debug header via 2x5 breakout + jumper wires to Morpho CN7: PA13 TMS/SWDIO, PA14 TCK/SWCLK, PA15 TDI, PB3 TDO/SWO, NRST, plus VTref (3V3) and GND | 3.3 V | None: probe I/O is 3.3 V and the target is 3.3 V | **The J-Link owns TCK, TMS, TDI and nRESET; the target drives TDO; the target supplies VTref.** Remove both Nucleo CN2 jumpers so the on-board ST-LINK releases the SWD lines (verify in ST UM1724). Never connect two probes to one TAP. |
| Arty FPGA JTAG | Internal: FT2232H channel A ↔ XC7A35T TCK/TMS/TDI/TDO | 3.3 V | None | Owned by the on-board FTDI; the external J-Link is **not** connected here. |
| Optional MCU ↔ FPGA | USART1 PA9 (TX) / PA10 (RX) on Morpho ↔ Arty Pmod JA pins 1/2, plus GND wire | 3.3 V ↔ 3.3 V | None needed; 100 Ω series resistors for ringing/short protection | A 5 V MCU (e.g. Arduino Uno) **would** need a level shifter (e.g. TXS0108E or 74LVC245 powered at 3.3 V): Artix-7 I/O are not 5 V tolerant. |
| Grounding | USB shields/grounds via hub; explicit GND wire with every board-to-board signal | 0 V reference | - | One reference through the hub; avoid a second supply ground path (e.g. a separately grounded bench PSU) without checking for ground loops. |

**Device-specific pinout assumptions to verify before wiring:** Nucleo-F446RE Morpho pin positions (ST UM1724),
Arty A7 Pmod JA pin mapping and on-board series resistance (Digilent Arty A7 reference manual), J-Link EDU Mini
target-interface voltage and 10-pin pinout (SEGGER UM08001). The table records design assumptions based on
these boards' published pin maps; it has not been checked against physical hardware and is the first thing a
hardware review should verify.

## Why the JTAG adapter is not "another compute board"

A JTAG adapter is a **debug interface**: it shifts bits through the target's Test Access Port to halt the CPU,
read/write memory and registers, program flash, or load an FPGA bitstream. It runs no user workload, has no
general-purpose I/O for an application, does not (here) power the target, and only works together with a specific
target, its voltage (VTref) and its pinout. So the workbench models it as a separate device type with different
capabilities (`jtag`, `swd`, `vtref-sense`, `no-compute`) instead of treating the three devices as
interchangeable boards. The three devices also do not share a connector (Mini-B / Micro-B / 10-pin 1.27 mm), a
protocol (UART vs JTAG/SWD vs JTAG-over-FTDI) or a core voltage (3.3 V MCU vs 1.0 V FPGA core).

## Mock adapters (C++)

`DeviceAdapter` interface: `descriptor()`, `capabilities()`, `connect()`, `disconnect()`, `state()`,
`readTelemetry()`. Three implementations (`MicrocontrollerMock`, `FpgaMock`, `JtagAdapterMock`) return
deterministic simulated telemetry in one shared message type, `TelemetryFrame{device_id, sequence, fields[]}`.

`DeviceManager` adds discovery and **retry-on-timeout**: up to 3 attempts per read (simulated 250 ms timeout each).

* Transient fault (`inject-timeout`): attempt 1 times out, attempt 2 succeeds - recovered automatically.
* Persistent fault: all attempts time out, the device goes to `error`; reads are refused until **Reconnect**,
  which recovers it. Both paths are unit-tested (`hardware_timeout_recovery`) and E2E-tested in the UI.

Every API response carries `"simulated": true` and the UI labels every state "(simulated)".

## Safe integration plan (replacing mocks with hardware)

1. Read the three board manuals; confirm every row of the table above; photograph jumper settings.
2. Power each board alone from the hub; measure 3.3 V (and 1.0 V on the Arty test point) with a multimeter.
3. Implement real adapters behind the same interface, one at a time: `SerialPortAdapter` (Win32 COM port) for
   Nucleo and Arty UARTs; a J-Link adapter using SEGGER's SDK. The UI and tests do not change.
4. J-Link to Nucleo: wire **with both boards unpowered**, GND first; power up; check VTref reads 3.3 V before any
   connect; first operation is a read-only IDCODE scan (expected Cortex-M4 DP IDCODE 0x4BA00477 / 0x2BA01477 for SWD).
5. Board-to-board UART only after both ends are verified 3.3 V, with GND connected first and series resistors fitted.
6. ESD strap, no hot-plugging of the debug cable, one probe per TAP, current-limited USB hub ports.

## Limitations

No real enumeration (USB VID/PID), no serial I/O, no JTAG/SWD transactions, no bitstream loading or firmware
deployment. Telemetry values are synthetic. Pin assignments are assumptions pending datasheet review.
