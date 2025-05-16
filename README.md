# AC123 Shade Controller

A complete system for controlling AOK AM25 and AM35 roller shades through emulation of AC123-16D and AC123-06D RF remotes.

## Project Overview

This project enables unified control of multiple RF roller shades through a single Arduino interface. It precisely replicates the RF signals from original AOK remote controls, allowing control over shades normally operated by different remote models.

The system successfully emulates:
- AC123-16D (16-channel) remotes
- AC123-06D (6-channel) remotes

By capturing and analyzing the exact RF signal patterns of the original remotes, this system reproduces the precise timing and bit patterns needed for reliable control of AOK AM25 and AM35 roller shades.

## System Architecture

The system consists of three main components:

1. **Arduino Transmitter** (`shade_transmitter.ino`)
   - Handles precise RF signal generation
   - Implements multiple remote protocols with accurate timing
   - Receives commands via Serial connection (USB)

2. **Python Control Interface** (`control_shades.py`)
   - Provides a user-friendly command interface
   - Manages the shade database (shades.db)
   - Translates simple commands into Arduino protocol format
   - Supports individual shade control and scene control

3. **SQLite Database** (`shades.db`)
   - Stores all shade configuration details
   - Contains RF protocol details specific to each shade
   - Organizes shades into scene groups

### Signal Flow

```
User Command → Python Interface → Database Lookup → Arduino Command Formation → RF Signal Generation → Shade Operation
```

## Required Hardware

### Essential Components

- **Arduino Board**
  - Arduino UNO R3 (or compatible)
  - Requires at least 32KB flash memory and 2KB RAM

- **RF Transmitter**
  - FS1000A 433MHz RF Transmitter Module
  - [Purchase on Amazon](https://www.amazon.com/dp/B01DKC2EY4)
  - Connect to Digital Pin 10 on Arduino

- **Computer for Control Interface**
  - Any system capable of running Python 3.6+
  - USB connection to Arduino

### For Signal Analysis (Development Only)

- **RTL-SDR v3 Receiver**
  - Essential for accurate signal capture and analysis
  - [https://www.rtl-sdr.com/](https://www.rtl-sdr.com/)
  - Required for debugging and adding new shades

- **Software Tools**
  - [rtl_433](https://github.com/merbanan/rtl_433/blob/master/README.md) for signal capture with RTL-SDR device
    - For the best experience and full functionality, it is recommended to use a native Linux installation or a Linux VM with USB passthrough capabilities for working with RTL-SDR devices and `rtl_433`.  I've heard good things about WSL 2 on Windows 11, though I've had my own bad experiences on Windows 10 with WSL 2.
  - Signal analysis tool (like Universal Radio Hacker or PulseView) - though I just used the native https://triq.org/pdv/ links native within `rtl_433`.
  - [Arduino IDE](https://support.arduino.cc/hc/en-us/articles/360019833020-Download-and-install-Arduino-IDE) 
     - NOTE: I do NOT leverage the [rc-switch](https://docs.arduino.cc/libraries/rc-switch/) library.  I could never get it working with this remote protocol.  All RF bits are sent directly to the TX_PIN and timings defined in C++.
  - [SDR#](https://airspy.com/download/) - Only because I think it's cool to SEE and hear the amplified transmissions from the remote =)

## Installation

### Setting Up the Arduino

1. **Connect Hardware**
   - Connect FS1000A transmitter to Arduino:
     - DATA pin to Arduino Digital Pin 10
     - VCC to 5V
     - GND to GND

2. **Install Arduino Sketch**
   - Open `shade_transmitter.ino` in Arduino IDE
   - Upload sketch to Arduino board
   - Note the serial port in use (e.g., COM3, /dev/ttyACM0)

### Setting Up Python Interface

1. **Install Required Packages**
   ```bash
   pip install pyserial
   ```

2. **Configure Serial Port**
   - Open `control_shades.py`
   - Modify the `SERIAL_PORT` variable to match your Arduino's port:
     ```python
     SERIAL_PORT = '/dev/ttyACM0'  # Change to your Arduino's port
     ```

3. **Initialize Database**
   - The system requires a `shades.db` SQLite database
   - If starting fresh, create with the schema shown in the Database Schema section

### Adding Shades to the System

To add a new shade to the system:

1. **Capture Original Remote Signal**
   - Use RTL-SDR and rtl_433 to capture the remote signal
   - Identify key parameters (header bytes, identifier bytes, commands)

2. **Add to Database**
   - Use the Python interface command:
     ```
     add:<id>,<remote_id>,<remote_type>,<remote_key>,<remote_name>,<channel>,<location>,<room>,<facing>,<type>,<header>,<id_bytes>,<up>,<down>,<stop>,<common>,<scene_group>
     ```

## Command Reference

### Python Interface Commands

| Command | Description | Example |
|---------|-------------|---------|
| `u<n>` | Move shade n UP | `u1` |
| `d<n>` | Move shade n DOWN | `d1` |
| `s<n>` | STOP shade n | `s1` |
| `scene:<group>,<cmd>` | Execute command for all shades in scene | `scene:kitchen,u` |
| `add:<params>` | Add/update shade in database | `add:1,254,AC123-16D,1,Main,1,North,Living,North,AM35,5C2D0D39,FEFF,F469,F569,DC51,50,living` |
| `exit` | Exit the program | `exit` |

### Arduino Serial Commands

| Command | Description | Example |
|---------|-------------|---------|
| `TX:<prefix>,<header>,<id>,<cmd>,<type>,<common>,<isCC>,<cmdType>` | Transmit RF signal | `TX:FE,5C2D0D39,FEFF,F469,1,50,0,0` |
| `INFO` | Display Arduino status | `INFO` |

#### TX Command Parameters

1. `prefix`: Remote prefix byte (FE for 16-channel, FF for 6-channel)
2. `header`: First 4 bytes of command (e.g., 5C2D0D39)
3. `id`: 2 bytes unique to shade (e.g., FEFF)
4. `cmd`: Command bytes (e.g., F469 for UP)
5. `type`: Remote type (0=AC123-06D, 1=AC123-16D)
6. `common`: Common byte for second section (e.g., 50)
7. `isCC`: Channel type (1 if CC, 0 otherwise)
8. `cmdType`: Command type (0=UP, 1=DOWN, 2=STOP)

## Database Schema

The `shades.db` SQLite database has a single `shades` table with the following structure:

| Column | Type | Description |
|--------|------|-------------|
| shade_id | INTEGER | Unique identifier for the shade |
| remote_id | INTEGER | ID of the original remote |
| remote_type | TEXT | Remote model (AC123-06D or AC123-16D) |
| remote_key | TEXT | Key/identifier on original remote |
| remote_name | TEXT | Human-readable remote name |
| channel | TEXT | Channel on original remote |
| location | TEXT | Physical location description |
| room | TEXT | Room where shade is installed |
| facing | TEXT | Direction the window faces |
| type | TEXT | Shade model type (AM25 or AM35) |
| header_bytes | TEXT | First 4 bytes of RF command (hex) |
| identifier_bytes | TEXT | 2 bytes unique to the shade (hex) |
| up_command | TEXT | Command bytes for UP action (hex) |
| down_command | TEXT | Command bytes for DOWN action (hex) |
| stop_command | TEXT | Command bytes for STOP action (hex) |
| common_byte | TEXT | Variable byte for common section (hex) |
| scene_group | TEXT | Group name for scene control |

Example SQL to create the table:

```sql
CREATE TABLE "shades" (
    "shade_id" INTEGER PRIMARY KEY,
    "remote_id" INTEGER,
    "remote_type" TEXT,
    "remote_key" TEXT,
    "remote_name" TEXT,
    "channel" TEXT,
    "location" TEXT,
    "room" TEXT,
    "facing" TEXT,
    "type" TEXT,
    "header_bytes" TEXT,
    "identifier_bytes" TEXT,
    "up_command" TEXT,
    "down_command" TEXT,
    "stop_command" TEXT,
    "common_byte" TEXT,
    "scene_group" TEXT
);
```

## Troubleshooting

### No Response from Shade

1. **Verify Timing Parameters**
   - Different remote models require specific timing
   - AC123-06D: SHORT_PULSE=312μs, LONG_PULSE=592μs, etc.
   - AC123-16D: SHORT_PULSE=280μs, LONG_PULSE=588μs, etc.

2. **Check Remote Type**
   - Ensure the correct remote type (0 or 1) is specified
   - This affects timing and protocol structure

3. **Verify Signal with RTL-SDR**
   - Capture both original remote and Arduino signals
   - Compare waveforms for differences
   - Adjust timings in code to match original remote

4. **Range and Positioning**
   - Ensure Arduino transmitter is within range of shade
   - Position may affect signal quality

### Arduino Communication Errors

1. **Serial Port Issues**
   - Check serial port configuration in Python script
   - Verify Arduino is properly connected

2. **Protocol Format**
   - Ensure TX command format is correct
   - All parameters must be provided in correct order

3. **Arduino Reset**
   - If commands stop working, try resetting the Arduino
   - Serial buffer may become corrupted during extended use

### Database Issues

1. **Missing Shade Data**
   - Check if shade exists in database
   - Verify commands (up, down, stop) are configured

2. **Scene Control Problems**
   - Verify shade is assigned to correct scene_group
   - Check consistency in scene_group naming

## RF Signal Analysis Tips

1. **Use rtl_433 for Signal Capture**
   - Run in debug mode: `rtl_433 -A`
   - Capture to file if you like: `rtl_433 -A -a > capture.txt`
    - (Or be fast at switching terminal tabs/windows to stop the capture mode when you see your successful capture & open the triq.org/pdv link in your browser - note that the capture content is local to the URI)

2. **Timing Analysis**
   - Focus on pulse widths for SHORT, LONG, GAP timings
   - Check sync pulse durations carefully

3. **Protocol Structure**
   - UP/DOWN: Has two sections (command + common)
   - STOP: Simpler structure (command only)

4. **Common Issues**
   - AC123-06D and AC123-16D have different timing requirements
   - Initial sync pulse duration is critical
   - Bit encoding must match exactly (inverted PWM)

## License

This project is provided for educational and personal use.

---

*Created with the assistance of my buddies Claude AI & Grok AI*
