# Total Working Flow

## 1. BLE Connection
1. Scan for BLE devices
2. Find devices matching MAC or name
3. Connect to target device
4. Verify characteristic UUID
5. Send test command to confirm connection

## 2. Camera Initialization
1. Try opening camera with V4L2 backend
2. Fall back to default backend if V4L2 fails
3. Search indices 0-9 for available camera
4. Try GStreamer pipeline if all backends fail
5. Set resolution (width, height)
6. Set FPS
7. Warmup camera (grab test frames)
8. Verify frame capture works

## 3. ROI/Tracking Selection
1. Display live camera feed
2. Wait for user input ('s', 'a', or 'q')
3. If 's': Manual ROI selection
4. If 'a': Auto red car detection
5. Initialize tracker (CSRT/KCF) for manual mode
6. Enable motion/color detection for auto mode

## 4. Centerline Map Building
1. Convert frame to HSV
2. Detect red barriers (HSV thresholds)
3. Build convex hull from red pixels
4. Detect white markers inside hull
5. Combine red + white as obstacles
6. Apply morphological operations (open, close, dilate)
7. Flood fill from outside to find free space
8. Extract largest connected component
9. Fill small holes
10. Distance transform on free space
11. Extract centerline skeleton points
12. Smooth centerline with Gaussian kernel
13. Store centerline path

## 5. Multi-threaded Execution Start
1. Start camera thread (capture frames continuously)
2. Start tracking thread (process frames at 15Hz)
3. Start BLE thread (send commands at 50Hz)
4. Main thread runs UI loop (render at 30fps)

## 6. Camera Loop (Thread 1)
1. Open camera
2. Grab frame from camera
3. Lock mutex and update latestFrame
4. Push frame to queue (max 5 frames)
5. Notify tracking thread
6. Repeat until stop signal

## 7. Tracking Loop (Thread 2)
1. Wait for tracker ready signal
2. Get frame from queue
3. Apply 15Hz rate limiting
4. Detect road edges (left, center, right)
5. Build centerline map (every 30 frames)
6. Detect car using priority:
   - Motion detection (primary)
   - Color detection (fallback)
   - Manual tracker (if ROI selected)
7. If car detected:
   - Analyze with boundary detector
   - Generate control vector (speed, left, right)
   - Update latestControl
8. If no car: Set speed=0
9. Log status every 3 seconds
10. Repeat until stop signal

## 8. Car Detection - Motion Method
1. Apply background subtraction (MOG2)
2. Fast warmup (30 frames)
3. Threshold foreground mask
4. Median blur
5. Morphological open + close + dilate
6. Find contours
7. Filter by area (20-30000 px)
8. Filter by width/height (5-500 px)
9. Score by distance from last position
10. Pick best candidate
11. Calculate center and movement vector

## 9. Car Detection - Color Method
1. Convert frame to HSV
2. Apply red/orange HSV ranges (6 masks)
3. Combine all masks
4. Morphological cleanup
5. Find contours
6. Filter by area (20-20000 px)
7. Filter by width/height (5-500 px)
8. Filter by solidity (>0.15)
9. Score by distance from last position
10. Pick best candidate
11. Calculate center and movement vector

## 10. Boundary Analysis & Control
1. Convert frame to grayscale
2. Calculate car heading from movement vector
3. Cast 3 rays (-60°, 0°, +60° relative to heading)
4. Measure distance to black boundary for each ray
5. Find closest ray distance
6. If distance < evasive threshold:
   - Reduce speed by 50%
   - Turn away from obstacle
7. Else: Normal navigation based on center ray
8. If centerline available: Use pure pursuit
9. Return control vector (speed, left, right)

## 11. Pure Pursuit Control (with centerline)
1. Find closest point on centerline to car
2. Calculate lookahead point (110 pixels ahead)
3. Calculate angle to target
4. Calculate steering delta angle
5. Clamp to max angle (15°)
6. Apply deadzone (±10 bytes near center)
7. Apply moving average filter (5 frames)
8. Convert to steering byte (0-255)
9. Calculate speed reduction based on turn
10. Split steering into left/right turn values

## 12. BLE Command Loop (Thread 3)
1. Lock control mutex
2. Get latest control vector
3. Build BLE command packet:
   - Device identifier (6 bytes)
   - Light on/off (1 byte)
   - Speed (1 byte)
   - Right turn (1 byte)
   - Left turn (1 byte)
   - Padding (5 bytes)
4. Send command via BLE characteristic
5. Sleep 20ms (50Hz rate)
6. Repeat until stop signal

## 13. UI Rendering Loop (Main Thread)
1. Lock and get latest frame
2. Lock and get latest tracked object
3. Lock and get latest rays
4. Clone frame for display
5. Draw centerline (yellow polyline)
6. Draw car bounding box (red)
7. Draw car center point (red circle)
8. Draw movement arrow (green)
9. Draw rays (magenta lines)
10. Draw status text
11. Show window
12. Check for 'q' key press
13. Repeat at 30fps until stop

## 14. Graceful Shutdown
1. Set stop event flag
2. Join camera thread
3. Join tracking thread
4. Join BLE thread
5. Close camera
6. Send STOP command 10 times (50ms delay)
7. Wait 500ms for car to stop
8. Disconnect BLE
9. Destroy all windows
10. Exit program
