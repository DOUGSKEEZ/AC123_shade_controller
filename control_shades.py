import sqlite3
import serial
import time

# Serial configuration
SERIAL_PORT = '/dev/ttyACM0'  # Adjust to your Arduino's port (e.g., COM3 on Windows/ - or typcally on Pop.OS '/dev/ttyUSB0')
BAUD_RATE = 115200

# Connect to SQLite database
conn = sqlite3.connect('shades.db')
cursor = conn.cursor()

# Connect to Arduino
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
time.sleep(2)  # Wait for Arduino to initialize

def send_command(command):
    ser.write((command + '\n').encode())
    time.sleep(0.1)  # Small delay to ensure Arduino processes
    while ser.in_waiting:
        print(ser.readline().decode().strip())

# Transmit command for a shade
def transmit_shade(shade_id, cmd_type):
    cursor.execute("SELECT * FROM shades WHERE shade_id = ?", (shade_id,))
    row = cursor.fetchone()
    if not row:
        print(f"Shade {shade_id} not found")
        return
    
    shade_id, remote_id, remote_type, remote_key, remote_name, channel, location, room, facing, type_, header_bytes, identifier_bytes, up_command, down_command, stop_command, common_byte, scene_group = row
    
    # Select command based on cmd_type
    cmd = up_command if cmd_type == 'u' else down_command if cmd_type == 'd' else stop_command
    if cmd == 'FF FF':
        print(f"Command {cmd_type} not configured for shade {shade_id}")
        return
    
    # Determine remote type: 0 for 6-channel, 1 for 16-channel
    remote_type_val = 0 if remote_type == 'AC123-06D' else 1
    
    # Use remote_id as prefix (assuming it matches RF protocol)
    prefix = remote_id
    
    # Convert cmd_type to cmdType parameter (0=UP, 1=DOWN, 2=STOP)
    cmd_type_val = 0 if cmd_type == 'u' else 1 if cmd_type == 'd' else 2
    
    # Format TX command with new cmdType parameter
    is_cc = 1 if channel == 'CC' else 0
    tx_cmd = f"TX:{prefix:02X},{header_bytes.replace(' ', '')},{identifier_bytes.replace(' ', '')},{cmd.replace(' ', '')},{remote_type_val},{common_byte},{is_cc},{cmd_type_val}"
    send_command(tx_cmd)

# Execute scene command
def execute_scene(scene_group, cmd_type):
    cursor.execute("SELECT shade_id FROM shades WHERE scene_group = ?", (scene_group,))
    shade_ids = [row[0] for row in cursor.fetchall()]
    if not shade_ids:
        print(f"No shades found for scene group: {scene_group}")
        return
    
    print(f"Executing {cmd_type} for scene group: {scene_group}")
    for shade_id in shade_ids:
        transmit_shade(shade_id, cmd_type)

# Command-line interface
def main():
    send_command("INFO")  # Check Arduino status
    print("Shade Control System")
    print("u[n] - UP for shade n")
    print("d[n] - DOWN for shade n")
    print("s[n] - STOP for shade n")
    print("scene:<group>,<cmd> - Execute command for scene group")
    print("add:<id>,<remote_id>,<remote_type>,<remote_key>,<remote_name>,<channel>,<location>,<room>,<facing>,<type>,<header>,<id_bytes>,<up>,<down>,<stop>,<common>,<scene_group> - Add shade")
    print("exit - Quit")
    
    while True:
        cmd = input("> ").strip()
        if cmd == "exit":
            break
        elif cmd.startswith("add:"):
            parts = cmd[4:].split(',')
            if len(parts) != 17:
                print("Invalid add command format")
                continue
            shade_id, remote_id, remote_type, remote_key, remote_name, channel, location, room, facing, type_, header, id_bytes, up, down, stop, common, scene_group = parts
            cursor.execute('''
                INSERT OR REPLACE INTO shades (
                    shade_id, remote_id, remote_type, remote_key, remote_name, channel,
                    location, room, facing, type, header_bytes, identifier_bytes,
                    up_command, down_command, stop_command, common_byte, scene_group
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ''', (
                int(shade_id), int(remote_id), remote_type, remote_key, remote_name, channel,
                location, room, facing, type_, header, id_bytes, up, down, stop, common, scene_group
            ))
            conn.commit()
            print(f"Shade {shade_id} added/updated")
        elif cmd.startswith("scene:"):
            parts = cmd[6:].split(',')
            if len(parts) != 2:
                print("Invalid scene command format")
                continue
            scene_group, cmd_type = parts
            if cmd_type not in ['u', 'd', 's']:
                print("Invalid scene command (use u, d, s)")
                continue
            execute_scene(scene_group, cmd_type)
        elif cmd.startswith("u") or cmd.startswith("d") or cmd.startswith("s"):
            cmd_type = cmd[0]
            shade_id = int(cmd[1:])
            transmit_shade(shade_id, cmd_type)
        else:
            print("Unknown command")

# Cleanup
try:
    main()
finally:
    ser.close()
    conn.close()