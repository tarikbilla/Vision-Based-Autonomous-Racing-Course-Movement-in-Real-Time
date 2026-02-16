#!/usr/bin/env python3
"""
BLE Test Script for DRIFT RC Car - Python version using simplepyble
This script connects to the DRIFT RC car and sends start/stop commands.

Installation:
    pip3 install simplepyble

Usage:
    python3 test_ble_python.py [device_name_or_mac] [characteristic_uuid]
    
Example:
    python3 test_ble_python.py "Echo Dot-R3J"
    python3 test_ble_python.py "f9:af:3c:e2:d2:f5"
"""

import sys
import time
import simplepyble

def int_to_hex(value, digits=4):
    """Convert integer to hexadecimal string with padding."""
    return format(value, f'0{digits}x')

def generate_command(light_on, speed, left_turn, right_turn):
    """Generate BLE command string matching DRIFT car protocol."""
    device_identifier = 'bf0a00082800'
    
    # Calculate steering value (as per DRIFT protocol)
    if right_turn > 0:
        steering_value = right_turn
    elif left_turn > 0:
        steering_value = 255 - left_turn
    else:
        steering_value = 0
    
    drift_value = 0
    light_value = '0200' if light_on else '0000'
    checksum = '00'
    
    command = (device_identifier +
              int_to_hex(speed, 4) +
              int_to_hex(drift_value, 4) +
              int_to_hex(steering_value, 4) +
              light_value +
              checksum)
    
    return command

def hex_string_to_bytes(hex_string):
    """Convert hex string to bytes."""
    return bytes.fromhex(hex_string)

def find_drift_car(device_name_or_mac=None):
    """Find DRIFT car by name or MAC address."""
    print("="*60)
    print("Scanning for BLE devices...")
    print("="*60)
    
    # Get adapters
    adapters = simplepyble.Adapter.get_adapters()
    if len(adapters) == 0:
        print("Error: No BLE adapters found!")
        print("Make sure Bluetooth is enabled on your Mac.")
        return None, None
    
    adapter = adapters[0]
    print(f"Using adapter: {adapter.identifier()}")
    print(f"Scanning for 10 seconds (make sure car is ON)...")
    print()
    
    # Scan for devices
    adapter.scan_for(10000)  # Scan for 10 seconds
    peripherals = adapter.scan_get_results()
    
    print(f"Found {len(peripherals)} BLE devices:")
    print("-" * 60)
    
    # List all devices
    drift_candidates = []
    for i, peripheral in enumerate(peripherals):
        name = peripheral.identifier()
        address = peripheral.address()
        print(f"  {i+1}. Name: '{name}' | MAC: {address}")
        
        # Check if this might be the DRIFT car
        name_lower = name.lower()
        name_upper = name.upper()
        address_lower = address.lower()
        
        is_drift = False
        if device_name_or_mac:
            # Check if matches provided name or MAC
            if (device_name_or_mac.lower() in name_lower or 
                device_name_or_mac.upper() in name_upper or
                device_name_or_mac.lower() == address_lower):
                is_drift = True
        else:
            # Auto-detect: Look for DRIFT-related names
            if ('drift' in name_lower or 
                'dr!ft' in name_lower or
                name.startswith('DRiFT') or
                name.startswith('DRIFT') or
                name.startswith('DR!FT') or
                'ED5C2384488D' in name_upper or  # Red car MAC in name
                'F9AF3CE2D2F5' in name_upper):    # Gray car MAC in name
                is_drift = True
        
        if is_drift:
            drift_candidates.append((i, peripheral, name, address))
            print(f"       DRIFT car candidate found!")
    
    print("-" * 60)
    
    if not drift_candidates:
        print("\n first try with mobile to find this device in the bro cancel GIG Indi clever pocket friendly means No DRIFT car found!")
        print("\nTroubleshooting:")
        print("  1. Make sure your car is powered ON")
        print("  2. Make sure car is in pairing/discoverable mode")
        print("  3. Check if car shows as 'Echo Dot-R3J' or similar")
        print("  4. Try specifying the device name or MAC address:")
        print("     python3 test_ble_python.py 'Echo Dot-R3J'")
        print("     python3 test_ble_python.py 'f9:af:3c:e2:d2:f5'")
        return None, None
    
    # Use first candidate
    idx, peripheral, name, address = drift_candidates[0]
    print(f"\n Found DRIFT car:")
    print(f"   Name: {name}")
    print(f"   MAC: {address}")
    
    return peripheral, address

def connect_to_car(peripheral, characteristic_uuid):
    """Connect to BLE car and find characteristic."""
    if peripheral is None:
        return None, None
    
    print("\n" + "="*60)
    print("Connecting to DRIFT car...")
    print("="*60)
    
    try:
        peripheral.connect()
        print(" Connected successfully!")
        
        # Small delay to allow service discovery
        time.sleep(1)
        
        # Discover services
        services = peripheral.services()
        print(f"\nFound {len(services)} service(s):")
        print("-" * 60)
        
        target_characteristic = None
        all_characteristics = []
        
        for service in services:
            print(f"\nService UUID: {service.uuid()}")
            characteristics = service.characteristics()
            print(f"  Characteristics ({len(characteristics)}):")
            
            for char in characteristics:
                char_uuid = char.uuid()
                print(f"    - {char_uuid}")
                all_characteristics.append((service.uuid(), char_uuid))
                
                # Check if this is our target characteristic
                if char_uuid.lower() == characteristic_uuid.lower():
                    target_characteristic = (service.uuid(), char_uuid)
                    print(f"       MATCH! This is the control characteristic")
        
        print("-" * 60)
        
        if target_characteristic is None:
            print(f"\n Warning: Target characteristic '{characteristic_uuid}' not found!")
            print("\nAvailable characteristics listed above.")
            print("\nTrying to use first writable characteristic...")
            
            # Try to find any writable characteristic
            for service_uuid, char_uuid in all_characteristics:
                try:
                    # Try to write a test (will fail but tells us if writable)
                    test_bytes = bytes([0x00])
                    # Don't actually write, just check if we can
                    target_characteristic = (service_uuid, char_uuid)
                    print(f"Will try: {char_uuid}")
                    break
                except:
                    continue
        
        if target_characteristic:
            print(f"\n Using characteristic: {target_characteristic[1]}")
        else:
            print("\n No suitable characteristic found!")
            print("You may need to check the DRIFT app or manual for the correct UUID.")
        
        return peripheral, target_characteristic
        
    except Exception as e:
        print(f"\n Error connecting: {e}")
        print("\nTroubleshooting:")
        print("  1. Make sure car is not connected to another device (phone app)")
        print("  2. Disconnect from DRIFT app first")
        print("  3. Try turning car off and on again")
        return None, None

def send_command(peripheral, characteristic, command_hex):
    """Send BLE command."""
    if peripheral is None:
        print(" Error: Not connected!")
        return False
    
    if characteristic is None:
        print(" Error: No characteristic selected!")
        return False
    
    try:
        command_bytes = hex_string_to_bytes(command_hex)
        service_uuid, char_uuid = characteristic
        
        # Try write_request first (preferred)
        try:
            peripheral.write_request(service_uuid, char_uuid, command_bytes)
        except:
            # Fallback to write_command if write_request fails
            try:
                peripheral.write_command(service_uuid, char_uuid, command_bytes)
            except Exception as e:
                print(f" Write error: {e}")
                return False
        
        return True
    except Exception as e:
        print(f" Error sending command: {e}")
        return False

def interactive_mode(peripheral, characteristic):
    """Interactive command mode."""
    print("\n" + "="*60)
    print("🎮 Interactive BLE Control Mode")
    print("="*60)
    print("Commands:")
    print("  s - Start (forward, speed 20)")
    print("  t - Stop")
    print("  l - Left turn")
    print("  r - Right turn")
    print("  f - Forward straight")
    print("  b - Reverse")
    print("  + - Increase speed")
    print("  - - Decrease speed")
    print("  q - Quit")
    print("="*60)
    
    speed = 0
    left = 0
    right = 0
    light = 0
    
    while True:
        try:
            cmd = input("\n> ").strip().lower()
            
            if not cmd:
                continue
            
            if cmd == 's':
                speed = 5
                light = 1
                left = 0
                right = 0
                print(" START: Speed=20, Lights ON")
            elif cmd == 't':
                speed = 0
                light = 0
                left = 0
                right = 0
                print(" STOP")
            elif cmd == 'l':
                left = 20
                right = 0
                if speed == 0:
                    speed = 5
                print(f"⬅️  LEFT TURN: Left={left}")
            elif cmd == 'r':
                right = 20
                left = 0
                if speed == 0:
                    speed = 5
                print(f"➡️  RIGHT TURN: Right={right}")
            elif cmd == 'f':
                left = 0
                right = 0
                if speed == 0:
                    speed = 20
                print("⬆️  FORWARD STRAIGHT")
            elif cmd == 'b':
                speed = 255 - 20  # Reverse
                left = 0
                right = 0
                print(f"⬇️  REVERSE: Speed={255-speed}")
            elif cmd == '+':
                if speed > 0 and speed < 255:
                    speed = min(255, speed + 10)
                    print(f"⚡ Speed increased to: {speed}")
            elif cmd == '-':
                if speed > 0:
                    speed = max(0, speed - 10)
                    print(f"🐌 Speed decreased to: {speed}")
            elif cmd == 'q':
                print(" Quitting...")
                # Send stop command
                stop_cmd = generate_command(0, 0, 0, 0)
                print(f"Sending STOP command: {stop_cmd}")
                send_command(peripheral, characteristic, stop_cmd)
                time.sleep(0.2)
                break
            else:
                print(" Unknown command. Type 'q' to quit.")
                continue
            
            # Generate and send command
            command = generate_command(light, speed, left, right)
            print(f" Command: {command}")
            
            if send_command(peripheral, characteristic, command):
                print(" Command sent successfully")
            else:
                print(" Failed to send command")
            
            # Small delay
            time.sleep(0.05)
            
        except KeyboardInterrupt:
            print("\n\n Interrupted. Sending STOP command...")
            stop_cmd = generate_command(0, 0, 0, 0)
            send_command(peripheral, characteristic, stop_cmd)
            break

def main():
    # Default DRIFT car characteristic UUID
    characteristic_uuid = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
    
    # Get device name/MAC from command line or use auto-detect
    device_name_or_mac = None
    if len(sys.argv) > 1:
        device_name_or_mac = sys.argv[1]
    if len(sys.argv) > 2:
        characteristic_uuid = sys.argv[2]
    
    print("="*60)
    print("🚗 DRIFT RC Car BLE Control")
    print("="*60)
    if device_name_or_mac:
        print(f"Looking for: {device_name_or_mac}")
    else:
        print("Auto-detecting DRIFT car...")
    print(f"Characteristic UUID: {characteristic_uuid}")
    print("="*60)
    print()
    
    # Find car
    peripheral, mac_address = find_drift_car(device_name_or_mac)
    
    if peripheral is None:
        print("\n Could not find DRIFT car. Exiting.")
        return 1
    
    # Connect
    peripheral, characteristic = connect_to_car(peripheral, characteristic_uuid)
    
    if peripheral is None:
        print("\n Failed to connect. Exiting.")
        return 1
    
    if characteristic is None:
        print("\n Warning: Could not find characteristic.")
        print("You can still try to send commands, but they may not work.")
        print("Press Enter to continue anyway, or Ctrl+C to exit...")
        try:
            input()
        except KeyboardInterrupt:
            return 1
    
    # Test commands
    print("\n" + "="*60)
    print("🧪 Testing Commands")
    print("="*60)
    
    # Stop
    stop_cmd = generate_command(0, 0, 0, 0)
    print(f"\n1. STOP command: {stop_cmd}")
    if send_command(peripheral, characteristic, stop_cmd):
        print("    Sent")
    time.sleep(0.5)
    
    # Start
    start_cmd = generate_command(1, 20, 0, 0)
    print(f"\n2. START command: {start_cmd}")
    print("   (Car should move forward for 2 seconds)")
    if send_command(peripheral, characteristic, start_cmd):
        print("    Sent")
    time.sleep(2)
    
    # Stop again
    print(f"\n3. STOP command: {stop_cmd}")
    if send_command(peripheral, characteristic, stop_cmd):
        print("    Sent")
    time.sleep(0.5)
    
    # Interactive mode
    print("\n" + "="*60)
    print("Entering interactive mode...")
    print("="*60)
    interactive_mode(peripheral, characteristic)
    
    # Disconnect
    print("\n" + "="*60)
    print("Disconnecting...")
    print("="*60)
    try:
        peripheral.disconnect()
        print(" Disconnected.")
    except Exception as e:
        print(f" Disconnect warning: {e}")
    
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n\n Interrupted by user.")
        sys.exit(1)
    except Exception as e:
        print(f"\n\n Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
