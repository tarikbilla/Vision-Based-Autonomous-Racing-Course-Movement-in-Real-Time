# Scan and Connect Guide - Step by Step

## 🎯 New Improved Script: `scan_and_connect.py`

This script provides a complete step-by-step process:

### Step 1: Scan All Bluetooth Devices
- Scans for 10 seconds
- Lists **ALL** discovered Bluetooth devices
- Shows device names and MAC addresses
- Marks potential DRIFT cars with 🚗

### Step 2: Select Device
- You choose which device to connect to
- Enter the device number
- Script confirms your selection

### Step 3: Connect
- Connects to your selected device
- Shows connection status

### Step 4: Discover Services
- Discovers all BLE services
- Lists all characteristics
- Shows properties (READ, WRITE, NOTIFY)
- Highlights writable characteristics

### Step 5: Select Characteristic
- You choose which characteristic to use
- Shows default DRIFT characteristic if found
- Can select any characteristic

### Step 6: Test Commands
- Automatically tests STOP → START → STOP
- Car should move forward for 2 seconds

### Step 7: Interactive Control
- Full control mode
- Send commands in real-time

## 🚀 How to Use

### Basic Usage:
```bash
python3 scan_and_connect.py
```

### What You'll See:

```
======================================================================
🔍 Scanning for ALL Bluetooth Devices...
======================================================================
📡 Using adapter: Default Adapter
⏳ Scanning for 10 seconds...
   (Make sure your car is ON and nearby)

======================================================================
✅ Found 5 Bluetooth device(s):
======================================================================

🚗  1. Name: 'Echo Dot-R3J'
      MAC: f9:af:3c:e2:d2:f5 [DRIFT CAR?]

📱  2. Name: 'iPhone'
      MAC: aa:bb:cc:dd:ee:ff

📱  3. Name: 'AirPods'
      MAC: 11:22:33:44:55:66

...

======================================================================
📋 Select Device to Connect
======================================================================

Enter device number (1-5) or 'q' to quit: 1

✅ Selected: Echo Dot-R3J [f9:af:3c:e2:d2:f5]

======================================================================
🔌 Connecting to Device...
======================================================================
Connecting...
✅ Connected successfully!

======================================================================
🔍 Discovering Services and Characteristics...
======================================================================

Found 2 service(s):
----------------------------------------------------------------------

[1] Service UUID: 6e400001-b5a3-f393-e0a9-e50e24dcca9e
    Characteristics (2):
      [1] 6e400002-b5a3-f393-e0a9-e50e24dcca9e
          Properties: WRITE
      [2] 6e400003-b5a3-f393-e0a9-e50e24dcca9e
          Properties: READ, NOTIFY

----------------------------------------------------------------------

======================================================================
📋 Select Characteristic for Control
======================================================================

💡 Writable characteristics (recommended):
  1. 6e400002-b5a3-f393-e0a9-e50e24dcca9e
     Service: 6e400001-b5a3-f393-e0a9-e50e24dcca9e

All characteristics:
⭐  1. 6e400002-b5a3-f393-e0a9-e50e24dcca9e
    2. 6e400003-b5a3-f393-e0a9-e50e24dcca9e

💡 Default DRIFT characteristic found at #1

Enter characteristic number (1-2, default=1) or 'q' to quit: 

✅ Selected: 6e400002-b5a3-f393-e0a9-e50e24dcca9e

======================================================================
🧪 Testing Commands
======================================================================

1. STOP command: bf0a00082800000000000000000000000000
   ✅ Sent

2. START command: bf0a00082800001400000000000000020000
   ✅ Sent
   (Car should move forward for 2 seconds)

3. STOP command: bf0a00082800000000000000000000000000
   ✅ Sent

======================================================================
🎮 Interactive Car Control
======================================================================
Commands:
  s - Start (forward, speed 20)
  t - Stop
  l - Left turn
  r - Right turn
  f - Forward straight
  b - Reverse
  + - Increase speed
  - - Decrease speed
  q - Quit
======================================================================

> s
🚗 START: Speed=20, Lights ON
📤 Command: bf0a00082800001400000000000000020000
✅ Command sent successfully

> t
🛑 STOP
📤 Command: bf0a00082800000000000000000000000000
✅ Command sent successfully

> q
👋 Quitting...
```

## 📋 Prerequisites

1. **Install simplepyble**:
   ```bash
   pip3 install simplepyble
   ```

2. **Enable Bluetooth** on Mac:
   - System Settings → Bluetooth → ON

3. **Prepare your car**:
   - Turn ON the DRIFT car
   - Make sure it's in pairing mode
   - Disconnect from phone app if connected

## 🎯 Key Features

✅ **Shows ALL devices** - Not just DRIFT cars  
✅ **Interactive selection** - You choose what to connect to  
✅ **Service discovery** - Shows all available services  
✅ **Characteristic selection** - Choose the right one  
✅ **Property detection** - Shows READ/WRITE/NOTIFY  
✅ **Auto-detection** - Marks DRIFT cars automatically  
✅ **Default detection** - Finds default DRIFT characteristic  
✅ **Error handling** - Clear error messages  

## 🔧 Troubleshooting

### "No BLE adapters found"
- Enable Bluetooth in System Settings
- Restart Bluetooth: `sudo pkill bluetoothd`

### "No Bluetooth devices found"
- Make sure devices are powered ON
- Move closer to devices
- Wait for scan to complete (10 seconds)

### "Connection failed"
- Disconnect from phone app first
- Turn car off and on again
- Make sure car is in pairing mode

### "No characteristics found"
- Wait a moment after connection
- Some devices need time to discover services
- Try reconnecting

### "Command sending failed"
- Make sure you selected a WRITE characteristic
- Check if car is still connected
- Try selecting a different characteristic

## 💡 Tips

1. **Look for 🚗 marker** - Marks potential DRIFT cars
2. **Choose WRITE characteristics** - Needed to send commands
3. **Use default if found** - Usually the correct one
4. **Test in safe area** - Car will move during testing
5. **Note the MAC address** - Useful for C++ integration

## ✅ Success Checklist

- [ ] Script scans and finds devices
- [ ] Your car appears in the list
- [ ] Connection successful
- [ ] Services discovered
- [ ] Characteristic selected
- [ ] Test commands work (car moves)
- [ ] Interactive mode works
- [ ] All commands respond correctly

## 🎉 Next Steps

Once this works:
1. Note the MAC address of your car
2. Note the characteristic UUID that works
3. Use this information for C++ integration
4. Test all commands to understand car behavior

## 📚 Comparison with Other Scripts

| Feature | scan_and_connect.py | test_ble_python.py |
|---------|-------------------|-------------------|
| Shows all devices | ✅ Yes | ⚠️ Only DRIFT |
| User selection | ✅ Yes | ❌ Auto-only |
| Service discovery | ✅ Full | ⚠️ Basic |
| Characteristic selection | ✅ Yes | ⚠️ Auto |
| Better for debugging | ✅ Yes | ❌ No |

**Use `scan_and_connect.py` for:**
- First-time setup
- Debugging connection issues
- Understanding device structure
- Testing different characteristics

**Use `test_ble_python.py` for:**
- Quick connection when you know the device
- Automated testing
- Integration scripts
