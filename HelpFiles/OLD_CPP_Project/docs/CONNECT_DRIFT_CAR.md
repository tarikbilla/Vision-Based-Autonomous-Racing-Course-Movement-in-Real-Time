# Connect to DRIFT Car - Step by Step Guide

## ✅ Updated Script Ready!

The `test_ble_python.py` script has been updated to:
- ✅ Search for "Echo Dot-R3J" by name
- ✅ Auto-detect DRIFT cars
- ✅ Show all discovered BLE devices
- ✅ Better error handling
- ✅ Try multiple characteristic UUIDs if needed

## 🚀 Quick Start

### Step 1: Make sure your car is ready
1. **Turn ON your DRIFT car**
2. **Make sure it's in pairing mode** (usually just being ON is enough)
3. **Disconnect from DRIFT phone app** if connected
4. **Keep car close** to your Mac (within 10 meters)

### Step 2: Enable Bluetooth on Mac
1. Open **System Settings** → **Bluetooth**
2. Make sure Bluetooth is **ON**
3. You should see "Echo Dot-R3J" in the device list

### Step 3: Install simplepyble (if not installed)
```bash
pip3 install simplepyble
```

### Step 4: Run the script
```bash
python3 test_ble_python.py "Echo Dot-R3J"
```

Or let it auto-detect:
```bash
python3 test_ble_python.py
```

## 📋 What the Script Does

1. **Scans for 10 seconds** - Finds all BLE devices
2. **Lists all devices** - Shows names and MAC addresses
3. **Finds your car** - Looks for "Echo Dot-R3J" or DRIFT-related names
4. **Connects** - Establishes BLE connection
5. **Discovers services** - Finds all BLE services and characteristics
6. **Tests commands** - Sends STOP, START, STOP automatically
7. **Interactive mode** - Lets you control the car

## 🎮 Interactive Commands

Once connected, you can use:
```
> s    # Start car (forward, speed 20)
> t    # Stop car
> l    # Turn left
> r    # Turn right
> f    # Forward straight
> b    # Reverse
> +    # Increase speed
> -    # Decrease speed
> q    # Quit (auto-stops car)
```

## 🔧 Troubleshooting

### "No BLE adapters found"
- Enable Bluetooth in System Settings
- Restart Bluetooth: `sudo pkill bluetoothd`

### "No DRIFT car found"
- Make sure car is ON
- Make sure car is in pairing mode
- Disconnect from phone app first
- Try specifying exact name: `python3 test_ble_python.py "Echo Dot-R3J"`
- Try with MAC address if you know it

### "Bluetooth is not enabled"
- System Settings → Bluetooth → Turn ON
- Grant permission to Terminal if prompted

### "Could not find characteristic"
- The script will list all available characteristics
- Try using a different UUID if the default doesn't work
- Check DRIFT app or manual for correct UUID

### "Connection failed"
- Make sure car is not connected to another device
- Turn car off and on again
- Try disconnecting from phone app first

## 📊 Expected Output

When it works, you'll see:
```
============================================================
🚗 DRIFT RC Car BLE Control
============================================================
Scanning for BLE devices...
Found 5 BLE devices:
  1. Name: 'Echo Dot-R3J' | MAC: f9:af:3c:e2:d2:f5
      ⭐ DRIFT car candidate found!

✅ Found DRIFT car:
   Name: Echo Dot-R3J
   MAC: f9:af:3c:e2:d2:f5

Connecting...
✅ Connected successfully!

Found 2 service(s):
Service UUID: 6e400001-b5a3-f393-e0a9-e50e24dcca9e
  Characteristics (2):
    - 6e400002-b5a3-f393-e0a9-e50e24dcca9e
      ⭐ MATCH! This is the control characteristic

✅ Using characteristic: 6e400002-b5a3-f393-e0a9-e50e24dcca9e

🧪 Testing Commands
1. STOP command: bf0a00082800000000000000000000000000
   ✅ Sent
2. START command: bf0a00082800001400000000000000020000
   ✅ Sent
   (Car should move forward for 2 seconds)
3. STOP command: bf0a00082800000000000000000000000000
   ✅ Sent

🎮 Interactive BLE Control Mode
> s    # You control the car here
```

## 🎯 Next Steps After Connection Works

1. **Test all commands** - Make sure start, stop, turns work
2. **Note the MAC address** - You'll need it for C++ integration
3. **Note the characteristic UUID** - Verify it matches
4. **Test different speeds** - See how car responds
5. **Integrate into C++ project** - Use the working Python code as reference

## 💡 Tips

- **Keep car close** during testing (within 5-10 meters)
- **Start with low speeds** for safety
- **Always stop before quitting** (script does this automatically)
- **Test in open area** with space for car to move
- **Note any issues** - Document car-specific behavior

## ✅ Success Checklist

- [ ] Car powered ON
- [ ] Car shows as "Echo Dot-R3J" in Mac Bluetooth settings
- [ ] Script finds the car
- [ ] Connection successful
- [ ] Characteristic found
- [ ] STOP command works
- [ ] START command works (car moves)
- [ ] Interactive mode works
- [ ] All commands respond correctly

## 🎉 You're Ready!

Run the script and connect to your DRIFT car:
```bash
python3 test_ble_python.py "Echo Dot-R3J"
```

Once it works, you can use this as a reference to integrate BLE into your C++ project!
