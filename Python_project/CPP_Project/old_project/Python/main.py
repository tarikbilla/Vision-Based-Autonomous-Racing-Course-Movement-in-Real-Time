import threading
import cv2
import math
import time
import pygame
import boundaryDetection
import numpy as np
import BTLEHandler as B
from tkinter import *
from tkinter.ttk import *
from PIL import ImageTk, Image
from queue import Empty, Full, Queue


####################################################################################################################
# Functions
####################################################################################################################

#subtask that does tracking with webcam
def tracking(returnq, frameq, bbox):
    """
    Tracks an object with go turn. 
    creates new window to select the car and to display tracking information. 
    Parameter:
        returnq (queue.Queue): returns tupel (position, last_movement_vector).
        frameq (queue.Queue): returns pic of the track (car and Boundaries visible).
        bbox : Tracking Boundary Box

    runs as a continues loop, has to be exit at the window with esc
    """

    midpoints = []

    # Open webcam
    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        print("[ERROR] Could not open webcam.")
        exit()

    # Initialize the GOTURN tracker
    tracker = cv2.TrackerGOTURN_create()
    print("[INFO] GOTURN model loaded successfully.")
    ret, frame = cap.read()
    ok = tracker.init(frame, bbox)
    print("[INFO] Tracking started. Press 'ESC' to exit.")

    # Tracking loop
    while True:
        ret, newframe = cap.read()
        if not ret:
            break
        #Puts frame in queue for futher use, discards oldest frame if queue is Full
        try:
            frameq.put_nowait(newframe)
        except Full:
            # Remove the oldest item and try again
            frameq.get_nowait()  # Discard the oldest item
            frameq.put_nowait(newframe)  # Retry adding the new item

        frame = newframe.copy()
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
            ret_obj = [midpoint, movement_vector]
             #Puts Position and Movement vector in queue for futher use, discards oldest if queue is Full
            try:
                returnq.put_nowait(ret_obj)
            except Full:
                # Remove the oldest item and try again
                returnq.get_nowait()  # Discard the oldest item
                returnq.put_nowait(ret_obj)  # Retry adding the new item

            # Display movement vector
            
            # Draw movement vector as an arrow
            predicted_point = (midpoint[0] + dx, midpoint[1] + dy)
            cv2.arrowedLine(frame, midpoint, predicted_point, (0, 255, 255), 2)

            #calculate Vector information
            magnitude = round(math.hypot(dx, dy),4)
            angle = round(math.degrees(math.atan2(dy, dx)),4)

            amp = 100 
            rayang = 60
            #calculate tracing lines -> move to tracking part maybe
            strait = (int(math.cos(math.radians(angle))*amp)+midpoint[0],int(math.sin(math.radians(angle))*amp)+midpoint[1])
            left = (int(math.cos(math.radians(angle - rayang))*amp)+midpoint[0],int(math.sin(math.radians(angle - rayang))*amp)+midpoint[1])
            right = (int(math.cos(math.radians(angle + rayang))*amp)+midpoint[0],int(math.sin(math.radians(angle + rayang))*amp)+midpoint[1])
            cv2.line(frame, midpoint, strait, (0, 0, 255), 2)
            cv2.line(frame, midpoint, left, (0, 0, 255), 2)
            cv2.line(frame, midpoint, right, (0, 0, 255), 2)
            # Print movement vector
            cv2.putText(frame, f"Vector: ({magnitude} + {angle} deg)", (50, 50), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)

        # Display tracking information
        cv2.putText(frame, f"Position: ({midpoint[0]}, {midpoint[1]})", (50, 80), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)
        cv2.putText(frame, "GOTURN Tracker", (50, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.75, (50, 170, 50), 2)
        cv2.imshow("Tracking", frame)
        
        # Press ESC to exit
        if cv2.waitKey(1) & 0xFF == 27:
            break

        # Check if window is closed
        if cv2.getWindowProperty("Tracking", cv2.WND_PROP_VISIBLE) < 1:
            break

        global stop_flag
        if stop_flag:
            # Cleanup
            cap.release()
            cv2.destroyAllWindows()
            print("[INFO] Tracking stopped.")
            return

    # Cleanup
    cap.release()
    cv2.destroyAllWindows()
    print("[INFO] Tracking stopped.")

# Boundary detection
def track_bounddarys(pos_queue: Queue, image_queue: Queue, result_queue: Queue):
    """
    Creates commands to steer the car throughout the racetrack
    
    Parameter:
        pos_queue (queue.Queue): contaions tupel (position, last_movement_vector).
        image_queue (queue.Queue): pic of the track (car and Boundaries visible).
        result_queue (queue.Queue): returns calculated steering commands.
        
    runs as continues loop in a thread
    """
    try:
        # Warte auf ein neues OpenCV-Frame des Tracks
        track_image = image_queue.get(timeout=0.1)
        track = pygame.image.frombuffer(track_image.tobytes(), track_image.shape[1::-1], "BGR")
    except Empty:
        main_screen.after(1,func= lambda: track_bounddarys(pos_queue,image_queue,result_queue))


    try:
        # Warte auf neue Positionsdaten (Position und letzter Bewegungsvektor)
        pos_data = pos_queue.get(timeout=0.1)
        position, last_movement = pos_data  #position = (x, y)
    except Empty:
        main_screen.after(1,func= lambda: track_bounddarys(pos_queue,image_queue,result_queue))

    
    car = boundaryDetection.Car(position, 10, 8)
    running = True
    while running:
        try:
            # Warte auf neue Positionsdaten (Position und letzter Bewegungsvektor)
            pos_data = pos_queue.get(timeout=0.1)
            position, last_movement = pos_data  #position = (x, y)
        except Empty:
            continue

        try:
            # Warte auf ein neues OpenCV-Frame des Tracks
            track_image = image_queue.get(timeout=0.1)
            track = pygame.image.frombuffer(track_image.tobytes(), track_image.shape[1::-1], "BGR")
        except Empty:
            continue
        
        
        # Update car
        car.update_real(last_movement,position,track)

        retary = car.get_boundary_data(car)
        try:
            result_queue.put_nowait(retary)
        except Full:
            # Remove the oldest item and try again
            result_queue.get_nowait()  # Discard the oldest item
            result_queue.put_nowait(retary)  # Retry adding the new item
        global stop_flag
        if stop_flag:
            return

# Car control    
def car_controller(DeviceHandler,queue_input):
    """
    Thread that sends commands to Dr!ft Cars, uses BTLEHandler
    Parameters:
        DeviceHandler: BLE Device handle 
        queue_input: input to steer the car, array of 4
            usage: [0] light control, [1] Speed, [2] Right value, [3] left value


    """
    B.DeviceHandler = DeviceHandler
    if (queue_input.empty()==True):
        main_screen.after(1,func= lambda: car_controller(DeviceHandler,queue_input))
        return
    len = queue_input.qsize()
    for i in range(0,len): 
        events = queue_input.get()
        if (events[0]):
            performlightOn(DeviceHandler)
        else:
            performlightOff(DeviceHandler)
        
        acceleration = events[1]
        if (acceleration > 0):
            if (acceleration > 100):
                acceleration = 100
            B.DeviceHandler.setSpeed((acceleration))
        elif (acceleration < 0):
            if acceleration < -100:
                acceleration = -100
            B.DeviceHandler.setReverseSpeed(abs(acceleration))
        else:
            B.DeviceHandler.setReverseSpeed(0)
        
        speed_bar['value'] = acceleration

        right_turn = events[2]
        left_turn = abs(events[3])
        if (right_turn>30):
            right_turn = 30
        if (left_turn>30):
            left_turn = 30


        uno_x = right_turn  
        right_bar['value'] = uno_x

        uno_x = left_turn 
        left_bar['value'] = uno_x

        if right_turn > 0:
            B.DeviceHandler.driveRight(right_turn)
        elif left_turn > 0:
            B.DeviceHandler.driveLeft(left_turn)
        else:
            B.DeviceHandler.driveRight(0)
        
    main_screen.after(1,func= lambda: car_controller(DeviceHandler,queue_input))

    
#################
# Helper
#################

# Function to rescale image
def rescale_image(image_path, width, height):
    # Open the image using Pillow
    pil_image = Image.open(image_path)
    # Resize the image
    resized_image = pil_image.resize((width, height), Image.Resampling.LANCZOS )
    # Convert back to PhotoImage for Tkinter
    return ImageTk.PhotoImage(resized_image)

# Car Lights 
def performlightOn(blehandler):
    #if (IsConnected != 0):
        blehandler.SET_LIGHT_VALUE = '0200'
        #recentCommand = B.DeviceHandler.generateAndSend();
        #print("Activating Lights")
        #print("Executed Command:" + recentCommand + '\n')

def performlightOff(blehandler):
        #if (IsConnected != 0):
            blehandler.SET_LIGHT_VALUE = '0000'
            #recentCommand = B.DeviceHandler.generateAndSend()
            #print("Deactivating Lights")
            #print("Executed Command:" + recentCommand + '\n')

#Sends data to the car, as a thread to reduce freezes in the UI
def send_data(handler):
    while 1:
        handler.generateAndSend()
        global stop_flag
        if stop_flag:
            return
        time.sleep(0.005)

# Connection to car
def car_connect(DeviceHandler,queue_input,mythr_2):
    Characteristics = DeviceHandler.connect()
    if (Characteristics):
        print(" => Connected to DR!FT Device!")
        mythr_2.start()
        main_screen.after(10,func= lambda: car_controller(DeviceHandler,queue_input))
    else:
        print(" => Failed to Connect!")

def startThread(DeviceHandler, thr1, thr2, cqueue):
    Characteristics = DeviceHandler.connect()
    if (Characteristics):
        print(" => Connected to DR!FT Device!")
        thr2.start()
        main_screen.after(10,func= lambda: car_controller(DeviceHandler,cqueue))
        cqueue.put([1,10,0,0])
        thr1.start()
    else:
        print(" => Failed to Connect!")


####################################################################################################################
# MAIN PROGRAM
####################################################################################################################

if __name__ == '__main__':
    DeviceHandler = B.BTLEHandler()
    speed_hex = '00'
    original_speed = 0
    original_direction = 0
    direction_hex = '00'
    reverseFlag = 0
    IsConnected = 0
    stop_flag = False
    ## Select tracking object:
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
    cap.release()

    #queues for threadsafe handling
    returnqueue = Queue(maxsize=20)
    framequeue = Queue(maxsize=20)
    controlqueue = Queue(maxsize=20)

    #start Tracking as a Thread:
    trackingThread = threading.Thread(target=tracking, args=(returnqueue,framequeue, bbox, ))
    trackingThread.start()

    #init borderdetection 
    consumer_thread = threading.Thread(target=track_bounddarys, args=(returnqueue,framequeue,controlqueue,), daemon=True)
    
    #init car thread, starts with button press
    car_thread = threading.Thread(target=send_data, args=(DeviceHandler, ))

    # Build UI
    main_screen = Tk()
    main_screen.geometry('1280x800')
    main_screen.title("Welcome to DR!FT cars")
    image1 = rescale_image("Python/photo/fondo.png", 1280, 800)  
    label_for_image = Label(main_screen, image=image1)
    label_for_image.place(x=0, y=0)

    app = Frame(width="390", height="100")
    app.place(x=230, y=200)

    # Create a label in the frame
    lmain = Label(app)
    lmain.grid()

    photo_center = rescale_image("Python/photo/Startengine.png", 100, 100)  
    Button(main_screen, text="center", image=photo_center, command=lambda: startThread(DeviceHandler, consumer_thread,car_thread,controlqueue)).place(x=700, y=550)


    s = Style()
    s.theme_use('default')
    s.configure("gold.Horizontal.TProgressbar", foreground="goldenrod", background="goldenrod", thickness=30)

    label6 = Label(text="Speed:", background="black", foreground="goldenrod", font=("Calibri"))
    label6.place(x=20, y=554)
    speed_bar = Progressbar(main_screen, style="gold.Horizontal.TProgressbar")
    speed_bar['length'] = 400
    speed_bar.place(x=90, y=550)

    label7 = Label(text="Left:", background="black", foreground="goldenrod", font=("Calibri"))
    label7.place(x=20, y=599)
    left_bar = Progressbar(main_screen, style="gold.Horizontal.TProgressbar")
    left_bar['length'] = 400
    left_bar.place(x=90, y=595)

    label5 = Label(text="Right:", background="black", foreground="goldenrod", font=("Calibri"))
    label5.place(x=20, y=644)
    right_bar = Progressbar(main_screen, style="gold.Horizontal.TProgressbar")
    right_bar['length'] = 400
    right_bar.place(x=90, y=640)

    try:
        main_screen.mainloop()
    except KeyboardInterrupt:
        stop_flag = True
        trackingThread.join()
        try:
            trackingThread.join()
            consumer_thread.join()
            car_thread.join()
        finally:
            stop_flag = True  # Signal the thread to stop
            trackingThread.join()  # Ensure it exits properly
            consumer_thread.join()
            car_thread.join() 
