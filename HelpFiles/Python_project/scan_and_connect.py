#!/usr/bin/env python3
"""
BLE Scanner and Connector for DRIFT RC Car
This script:
1. Scans and lists ALL Bluetooth devices
2. Lets you select which device to connect to
3. Connects and discovers services/characteristics
4. Sends control commands to the car

Installation:
    pip3 install simplepyble
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

def scan_all_bluetooth_devices():
    """Scan and list ALL Bluetooth devices."""
    print("="*70)
    print("🔍 Scanning for ALL Bluetooth Devices...")
    print("="*70)
    
    # Get adapters
    adapters = simplepyble.Adapter.get_adapters()
    if len(adapters) == 0:
        print(" Error: No BLE adapters found!")
        print("Make sure Bluetooth is enabled on your Mac.")
        return None, []
    
    adapter = adapters[0]
    print(f"📡 Using adapter: {adapter.identifier()}")
    print(f"⏳ Scanning for 10 seconds...")
    print("   (Make sure your car is ON and nearby)")
    print()
    
    # Scan for devices
    try:
        adapter.scan_for(10000)  # Scan for 10 seconds
        peripherals = adapter.scan_get_results()
    except Exception as e:
        print(f" Error during scan: {e}")
        print("Make sure Bluetooth is enabled in System Settings.")
        return None, []
    
    if len(peripherals) == 0:
        print(" No Bluetooth devices found!")
        print("\nTroubleshooting:")
        print("  1. Make sure Bluetooth is enabled")
        print("  2. Make sure devices are powered on")
        print("  3. Try moving closer to devices")
        return None, []
    
    print("="*70)
    print(f" Found {len(peripherals)} Bluetooth device(s):")
    print("="*70)
    print()
    
    # Display all devices
    device_list = []
    for i, peripheral in enumerate(peripherals):
        name = peripheral.identifier() if peripheral.identifier() else "(Unknown)"
        address = peripheral.address()
        
        # Check if this might be a DRIFT car
        name_lower = name.lower()
        name_upper = name.upper()
        is_drift = ('drift' in name_lower or 
                   'dr!ft' in name_lower or
                   name.startswith('DRiFT') or
                   name.startswith('DRIFT') or
                   name.startswith('DR!FT') or
                   'ED5C2384488D' in name_upper or  # Red car MAC in name
                   'F9AF3CE2D2F5' in name_upper)    # Gray car MAC in name
        
        marker = "🚗" if is_drift else "📱"
        drift_note = " [DRIFT CAR?]" if is_drift else ""
        
        print(f"{marker} {i+1:2d}. Name: '{name}'")
        print(f"      MAC: {address}{drift_note}")
        print()
        
        device_list.append({
            'index': i,
            'peripheral': peripheral,
            'name': name,
            'address': address,
            'is_drift': is_drift
        })
    
    print("="*70)
    return adapter, device_list

def select_device(device_list):
    """Let user select a device to connect to."""
    if not device_list:
        return None
    
    print("\n" + "="*70)
    print("📋 Select Device to Connect")
    print("="*70)
    
    while True:
        try:
            choice = input(f"\nEnter device number (1-{len(device_list)}) or 'q' to quit: ").strip()
            
            if choice.lower() == 'q':
                print("👋 Exiting...")
                return None
            
            try:
                device_num = int(choice)
                if 1 <= device_num <= len(device_list):
                    selected = device_list[device_num - 1]
                    print(f"\n Selected: {selected['name']} [{selected['address']}]")
                    return selected
                else:
                    print(f" Invalid number. Please enter 1-{len(device_list)}")
            except ValueError:
                print(" Invalid input. Please enter a number or 'q'")
        except KeyboardInterrupt:
            print("\n\n👋 Cancelled by user.")
            return None

def discover_services_and_characteristics(peripheral):
    """Discover all services and characteristics."""
    print("\n" + "="*70)
    print("🔍 Discovering Services and Characteristics...")
    print("="*70)
    
    try:
        # Small delay to allow service discovery
        time.sleep(1)
        
        services = peripheral.services()
        print(f"\nFound {len(services)} service(s):")
        print("-" * 70)
        
        all_characteristics = []
        
        for service_idx, service in enumerate(services):
            print(f"\n[{service_idx + 1}] Service UUID: {service.uuid()}")
            characteristics = service.characteristics()
            print(f"    Characteristics ({len(characteristics)}):")
            
            for char_idx, char in enumerate(characteristics):
                char_uuid = char.uuid()
                print(f"      [{char_idx + 1}] {char_uuid}")
                
                # Check properties
                properties = []
                if hasattr(char, 'can_read'):
                    if char.can_read():
                        properties.append("READ")
                if hasattr(char, 'can_write'):
                    if char.can_write():
                        properties.append("WRITE")
                if hasattr(char, 'can_notify'):
                    if char.can_notify():
                        properties.append("NOTIFY")
                
                if properties:
                    print(f"          Properties: {', '.join(properties)}")
                
                all_characteristics.append({
                    'service_uuid': service.uuid(),
                    'char_uuid': char_uuid,
                    'service_idx': service_idx,
                    'char_idx': char_idx,
                    'properties': properties
                })
        
        print("-" * 70)
        return all_characteristics
        
    except Exception as e:
        print(f" Error discovering services: {e}")
        return []

def select_characteristic(all_characteristics):
    """Let user select a characteristic to use."""
    if not all_characteristics:
        return None
    
    print("\n" + "="*70)
    print("📋 Select Characteristic for Control")
    print("="*70)
    
    # Show writable characteristics first
    writable_chars = [c for c in all_characteristics if 'WRITE' in c['properties']]
    
    if writable_chars:
        print("\n💡 Writable characteristics (recommended):")
        for i, char in enumerate(writable_chars):
            print(f"  {i+1}. {char['char_uuid']}")
            print(f"     Service: {char['service_uuid']}")
    
    print(f"\nAll characteristics:")
    for i, char in enumerate(all_characteristics):
        marker = "⭐" if 'WRITE' in char['properties'] else "  "
        print(f"{marker} {i+1:2d}. {char['char_uuid']}")
    
    # Default DRIFT characteristic
    default_uuid = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
    default_found = None
    for i, char in enumerate(all_characteristics):
        if char['char_uuid'].lower() == default_uuid.lower():
            default_found = i + 1
            break
    
    if default_found:
        print(f"\n💡 Default DRIFT characteristic found at #{default_found}")
    
    while True:
        try:
            if default_found:
                prompt = f"\nEnter characteristic number (1-{len(all_characteristics)}, default={default_found}) or 'q' to quit: "
            else:
                prompt = f"\nEnter characteristic number (1-{len(all_characteristics)}) or 'q' to quit: "
            
            choice = input(prompt).strip()
            
            if choice.lower() == 'q':
                return None
            
            if not choice and default_found:
                choice = str(default_found)
            
            try:
                char_num = int(choice)
                if 1 <= char_num <= len(all_characteristics):
                    selected = all_characteristics[char_num - 1]
                    print(f"\n Selected: {selected['char_uuid']}")
                    return (selected['service_uuid'], selected['char_uuid'])
                else:
                    print(f" Invalid number. Please enter 1-{len(all_characteristics)}")
            except ValueError:
                print(" Invalid input. Please enter a number or 'q'")
        except KeyboardInterrupt:
            print("\n\n👋 Cancelled by user.")
            return None

def connect_to_device(peripheral):
    """Connect to the selected device."""
    print("\n" + "="*70)
    print("🔌 Connecting to Device...")
    print("="*70)
    
    try:
        print("Connecting...")
        peripheral.connect()
        print(" Connected successfully!")
        return True
    except Exception as e:
        print(f" Connection failed: {e}")
        print("\nTroubleshooting:")
        print("  1. Make sure device is not connected to another device")
        print("  2. Try disconnecting from phone app first")
        print("  3. Turn device off and on again")
        return False

def send_command(peripheral, characteristic, command_hex):
    """Send BLE command."""
    if peripheral is None or characteristic is None:
        return False
    
    try:
        command_bytes = hex_string_to_bytes(command_hex)
        service_uuid, char_uuid = characteristic
        
        # Try write_request first (preferred)
        try:
            peripheral.write_request(service_uuid, char_uuid, command_bytes)
            return True
        except:
            # Fallback to write_command
            try:
                peripheral.write_command(service_uuid, char_uuid, command_bytes)
                return True
            except Exception as e:
                print(f" Write error: {e}")
                return False
    except Exception as e:
        print(f" Error sending command: {e}")
        return False

def interactive_control(peripheral, characteristic):
    """Interactive command mode."""
    print("\n" + "="*70)
    print("🎮 Interactive Car Control")
    print("="*70)
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
    print("="*70)
    
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
                speed = 20
                light = 1
                left = 0
                right = 0
                print("🚗 START: Speed=20, Lights ON")
            elif cmd == 't':
                speed = 0
                light = 0
                left = 0
                right = 0
                print("🛑 STOP")
            elif cmd == 'l':
                left = 20
                right = 0
                if speed == 0:
                    speed = 20
                print(f"⬅️  LEFT TURN: Left={left}")
            elif cmd == 'r':
                right = 20
                left = 0
                if speed == 0:
                    speed = 20
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
                print("👋 Quitting...")
                # Send stop command
                stop_cmd = generate_command(0, 0, 0, 0)
                print(f"Sending STOP command: {stop_cmd}")
                send_command(peripheral, characteristic, stop_cmd)
                time.sleep(0.2)
                break
            else:
                print("❓ Unknown command. Type 'q' to quit.")
                continue
            
            # Generate and send command
            command = generate_command(light, speed, left, right)
            print(f"📤 Command: {command}")
            
            if send_command(peripheral, characteristic, command):
                print(" Command sent successfully")
            else:
                print(" Failed to send command")
            
            time.sleep(0.05)
            
        except KeyboardInterrupt:
            print("\n\n  Interrupted. Sending STOP command...")
            stop_cmd = generate_command(0, 0, 0, 0)
            send_command(peripheral, characteristic, stop_cmd)
            break

def main():
    print("="*70)
    print("🚗 DRIFT RC Car BLE Scanner and Controller")
    print("="*70)
    print()
    
    # Step 1: Scan for all devices
    adapter, device_list = scan_all_bluetooth_devices()
    
    if adapter is None or not device_list:
        return 1
    
    # Step 2: Select device
    selected_device = select_device(device_list)
    if selected_device is None:
        return 0
    
    peripheral = selected_device['peripheral']
    
    # Step 3: Connect
    if not connect_to_device(peripheral):
        return 1
    
    # Step 4: Discover services and characteristics
    all_characteristics = discover_services_and_characteristics(peripheral)
    
    if not all_characteristics:
        print(" No characteristics found!")
        try:
            peripheral.disconnect()
        except:
            pass
        return 1
    
    # Step 5: Select characteristic
    characteristic = select_characteristic(all_characteristics)
    
    if characteristic is None:
        print(" No characteristic selected!")
        try:
            peripheral.disconnect()
        except:
            pass
        return 1
    
    # Step 6: Test commands
    print("\n" + "="*70)
    print("🧪 Testing Commands")
    print("="*70)
    
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
    
    # Step 7: Interactive mode
    interactive_control(peripheral, characteristic)
    
    # Step 8: Disconnect
    print("\n" + "="*70)
    print("🔌 Disconnecting...")
    print("="*70)
    try:
        peripheral.disconnect()
        print(" Disconnected.")
    except Exception as e:
        print(f"  Disconnect warning: {e}")
    
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n\n  Interrupted by user.")
        sys.exit(1)
    except Exception as e:
        print(f"\n\n Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
