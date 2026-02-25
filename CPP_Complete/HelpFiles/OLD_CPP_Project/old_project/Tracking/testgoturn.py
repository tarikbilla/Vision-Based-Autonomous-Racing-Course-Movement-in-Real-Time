import cv2
import math

midpoints = []

# Initialize the GOTURN tracker
tracker = cv2.TrackerGOTURN_create()


print("[INFO] GOTURN model loaded successfully.")

# Open webcam
cap = cv2.VideoCapture(0)
if not cap.isOpened():
    print("[ERROR] Could not open webcam.")
    exit()

# Read first frame
ret, frame = cap.read()
if not ret:
    print("[ERROR] Could not read from webcam.")
    cap.release()
    exit()

# Let the user select ROI
cv2.imshow("Select Object to Track", frame)
bbox = cv2.selectROI("Select Object to Track", frame, False)
cv2.destroyAllWindows()

# Initialize the tracker
ok = tracker.init(frame, bbox)


print("[INFO] Tracking started. Press 'ESC' to exit.")

# Tracking loop
while True:
    ret, frame = cap.read()
    if not ret:
        break

    # Update tracker
    ok, bbox = tracker.update(frame)

    # Get bounding box points
    p1 = (int(bbox[0]), int(bbox[1]))
    p2 = (int(bbox[0] + bbox[2]), int(bbox[1] + bbox[3]))

    # Draw bounding box
    cv2.rectangle(frame, p1, p2, (255, 0, 0), 2, 1)

    # Calculate midpoint
    mid_x = int(bbox[0] + bbox[2] / 2)
    mid_y = int(bbox[1] + bbox[3] / 2)
    midpoint = (mid_x, mid_y)
    midpoints.append(midpoint)

    # Draw midpoint on frame
    cv2.circle(frame, midpoint, 5, (0, 255, 0), -1)

    # Keep only the last N midpoints for tracking
    if len(midpoints) > 10:
        midpoints.pop(0)

    # Calculate movement vector if at least two midpoints exist
    if len(midpoints) >= 2:
        dx = midpoints[-1][0] - midpoints[-2][0]
        dy = midpoints[-1][1] - midpoints[-2][1]
        movement_vector = (dx, dy)

        # Draw movement vector as an arrow
        predicted_point = (midpoint[0] + dx, midpoint[1] + dy)
        cv2.arrowedLine(frame, midpoint, predicted_point, (0, 255, 255), 2)
        #calculate Vector information
        magnitude = round(math.hypot(dx, dy),2)
        angle = round(math.degrees(math.atan2(dy, dx)),2)

        # Display movement vector
        cv2.putText(frame, f"Vector: ({magnitude} + {angle} deg)", (50, 50), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)

    # Display tracking information
    cv2.putText(frame, f"Position: ({midpoint[0]}, {midpoint[1]})", (50, 80), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)
    cv2.putText(frame, "GOTURN Tracker", (50, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.75, (50, 170, 50), 2)
    cv2.imshow("Tracking", frame)

    # Press ESC to exit
    if cv2.waitKey(1) & 0xFF == 27:
        break

# Cleanup
cap.release()
cv2.destroyAllWindows()
print("[INFO] Tracking stopped.")
