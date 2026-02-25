import cv2
import numpy as np

img = cv2.imread("img_raw.png")

if img is None:
    print("Image not loaded!")
    exit()

img = cv2.resize(img, (800, 500))

hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)

lower_red1 = np.array([0, 70, 50])
upper_red1 = np.array([15, 255, 255])

lower_red2 = np.array([165, 70, 50])
upper_red2 = np.array([180, 255, 255])

mask1 = cv2.inRange(hsv, lower_red1, upper_red1)
mask2 = cv2.inRange(hsv, lower_red2, upper_red2)

mask = mask1 + mask2

kernel = np.ones((7, 7), np.uint8)

mask = cv2.dilate(mask, kernel, iterations=2)
mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)

print("Mask sum:", np.sum(mask))

# -----------------------------------
# 1️⃣ Fill region between borders
# -----------------------------------

inv_mask = cv2.bitwise_not(mask)

h, w = inv_mask.shape
flood = inv_mask.copy()
flood_mask = np.zeros((h+2, w+2), np.uint8)

cv2.floodFill(flood, flood_mask, (0, 0), 0)

track_region = cv2.bitwise_not(flood)

cv2.imwrite("track_region.png", track_region)

# -----------------------------------
# 2️⃣ Distance Transform
# -----------------------------------

dist = cv2.distanceTransform(track_region, cv2.DIST_L2, 5)

dist_norm = cv2.normalize(dist, None, 0, 255, cv2.NORM_MINMAX)
dist_norm = np.uint8(dist_norm)

cv2.imwrite("distance_map.png", dist_norm)

# -----------------------------------
# 3️⃣ Extract centerline
# -----------------------------------

_, centerline = cv2.threshold(dist_norm, 200, 255, cv2.THRESH_BINARY)

cv2.imwrite("centerline.png", centerline)

# -----------------------------------
# 4️⃣ Overlay centerline
# -----------------------------------

center_output = img.copy()

center_contours, _ = cv2.findContours(centerline, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
output = img.copy()
cv2.drawContours(center_output, center_contours, -1, (255, 0, 0), 2)



# -----------------------------------
# Save and display everything
# -----------------------------------
cv2.imshow("Contours", output)
cv2.imwrite("mask_result.png", mask)
cv2.imwrite("centerline_overlay.png", center_output)

cv2.imshow("Mask", mask)
cv2.imshow("Track Region", track_region)
cv2.imshow("Distance Map", dist_norm)
cv2.imshow("Centerline", center_output)

cv2.waitKey(0)
cv2.destroyAllWindows()
