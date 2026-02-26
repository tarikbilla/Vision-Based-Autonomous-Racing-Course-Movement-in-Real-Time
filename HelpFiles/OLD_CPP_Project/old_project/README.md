# Vision based autonomous racing course movement 
 
The relevance of real-time object tracking and autonomous navigation in a multitude of industries and circumstances such as surveillance, robotics, military, logistics and commercial as well as personal transport is beyond question. Typically, Inertial Measurement Units (IMUs) are used, which increases the costs involved by many magnitudes. This project shall aim to implement a GOTURN based tracking system to bring about real-time object detection and autonomous movement in a model car on a model car racing course. For this, vision-based tracking of a former project has been enhanced with an emphasis on the use Bluetooth Low Energy (BLE). 
A comparative study with other tracking algorithms such as KCF, MOSSE and CSRT demonstrates GOTURN’s superiority in efficient real-time tracking. GOTURN operates at up to 120 frames per second (FPS) which makes it more efficient and robust in comparison to other methods. [1] The information gained from the implementation of this project contributes to low-cost, real-time vision-based tracking.

# Code Description

# Main Package (Python)

![Untitled Diagram drawio(1)](https://github.com/user-attachments/assets/a2001e59-db59-4c22-bb7e-c849c3d7d686)
Figure 1, Flowchart of Main Package

The python package serves as the core logic for controlling a Bluetooth Low Energy (BLE) car, handling its movement, boundary detection, and user interface interactions. It integrates Bluetooth communication, image-based boundary detection, and a main script that ties all components together to create an interactive car simulation.
This package contains three main Python scripts:
•	BTLEHandler.py: Manages BLE communication with the car, handling speed, direction, and light controls.
•	boundaryDetection.py: Implements vision-based boundary detection, using image processing techniques to guide the car and avoid obstacles.
•	main.py: Serves as the main entry point, integrating BLE communication, boundary detection, and user input for real-time car control.
Additionally, the package includes two folders:
•	photo/: Stores images used for interface elements such as start engine graphics.
•	pycache/: Contains compiled Python files for performance optimization.

1.	BTLEHandler.py
Overview
The BTLEHandler class is responsible for establishing a Bluetooth Low Energy (BLE) connection with a remote-controlled car and sending steering, speed, and lighting commands. It utilizes the simplepyble library for BLE communication, scanning for devices, establishing a connection, and transmitting commands using characteristic UUIDs.
This module defines methods for controlling the car's speed, direction, and light settings while ensuring continuous command transmission. It follows a structured hexadecimal command format to communicate with the car’s onboard controller.
Key Components
Initialization (__init__)
- Defines BLE connection parameters, including the device MAC address, service characteristic UUID, and handle identifier.
- Initializes control parameters for speed, steering, and light settings.
- Sets a flag for continuous command transmission.
Connection Method
•	connect():
-	Scans for BLE devices and attempts to establish a connection with the specified MAC address.
-	Lists available services and characteristics.
-	Stores the characteristic UUID for communication.

4.  Conversion Utility
-	twoDigitHex(number): Converts a given integer into a 4-digit hexadecimal string for command formatting.

Control Methods
•	Speed Control:
-	setSpeed(speed): Sets the speed of the car (0-255).
-	setReverseSpeed(speed): Sets reverse speed using an inverse value formula.
-	increaseSpeed() / decreaseSpeed(): Adjusts speed incrementally.

•	Steering Control:
-	driveRight(rightValue): Turns the car to the right by increasing the right wheel value.
-	driveLeft(leftValue): Turns the car to the left by increasing the left wheel value.

Command Generation and Transmission
•	generateCommand(): Constructs the hexadecimal command string containing speed, direction, and lighting values.
•	generateAndSend(): Generates the command and sends it to the BLE device using the stored characteristic UUID.
Functionality Summary
The BTLEHandler module provides a structured way to control the BLE car by continuously sending formatted commands. It allows real-time adjustments to speed and steering while maintaining an active connection to the BLE peripheral.
2.	boundaryDetection.py
Overview :
The boundaryDetection.py module defines the Car and Ray classes, which simulate a vehicle navigating a track while detecting boundaries using a simple ray-based approach. The car utilizes sensor rays to detect obstacles and adjust its direction accordingly. The module is primarily used in autonomous navigation simulations where obstacle avoidance is required.
Key Components
Car Class
The Car class represents the vehicle, which moves along a track while avoiding obstacles by adjusting its direction based on sensor readings.
•	Initialization (__init__)
-	Initializes the car’s position, speed, direction, and decision-making logic.
-	Defines three sensor rays at angles of -60°, 0°, and +60°.
-	Sets up guidance mechanisms for steering adjustments.

•	Update Methods
-	update(track): Updates the car's position, sensor rays, and guidance based on track boundaries.
-	update_real(last_movement, position, track): Updates direction based on real movement data.
•	Boundary Detection & Guidance
-	get_boundary_data(car): Returns an array [1, speed, left_value, right_value] representing movement and direction values.
-	find_min_max_rays(): Identifies the closest and farthest boundary detected by the rays.
-	evasive_action(max_ray): Adjusts direction when a boundary is too close.
-	decision_counter_check(): Adds slight directional variation to avoid getting stuck.
•	Position and Movement
-	update_position(): Updates the car’s position based on its current direction and speed.
-	get_position(base_angle, length): Computes new position coordinates based on movement.

Ray Class
The Ray class represents a sensor ray that extends from the car to detect nearby obstacles.
•	Initialization (__init__)
-	Defines ray position, initial angle, maximum length, and direction.
-	Initializes distance measurement to track detected obstacles.
•	Ray Update Methods
-	update(point, direction, track): Updates the ray’s position, direction, and endpoint.
-	update_direction(direction): Adjusts the ray's angle based on the car’s direction.
-	update_terminus(track): Detects boundaries by scanning for dark pixels on the track.
•	Boundary Detection Logic
-	Rays scan ahead for obstacles within their range.
-	Dark pixels (low grayscale values) indicate a boundary, triggering evasive maneuvers.
-	If no boundary is detected, the ray extends to its maximum length.
Functionality Summary
The boundaryDetection.py module enables a simulated car to navigate autonomously while avoiding obstacles. Using three sensor rays, the car detects track boundaries and makes directional adjustments accordingly. This module is essential for developing and testing autonomous driving behaviors in simulated environments.
3.	main.py
Overview
The main.py module serves as the primary entry point for the Bluetooth Low Energy (BLE) car control system. It integrates BLE communication via the BTLEHandler module, vision-based boundary detection using boundaryDetection, and real-time user input processing. This script facilitates tracking, movement control, and interaction with the graphical user interface (GUI), ensuring seamless operation of the autonomous car simulation.
Key Components:
•	Initialization and Imports:
-	Imports essential libraries, including threading, OpenCV for image processing, Pygame for UI interaction, and tkinter for graphical elements.
-	Integrates BTLEHandler for BLE communication and boundaryDetection for navigation logic.
-	Defines queues for handling real-time data exchange between tracking, control, and GUI components.
•	Tracking and Vision-Based Control:
-	Implements object tracking using OpenCV’s GOTURN tracker to follow the car's movement and detect its position.
-	Continuously updates the car’s position and movement vectors to enable accurate steering adjustments.
-	Uses a queue-based approach to manage frame capture and movement data efficiently.
•	User Interface (GUI):
-	Initializes a tkinter-based interface for user interaction, including buttons for starting/stopping tracking and adjusting control settings.
-	Displays real-time camera feed for monitoring the car’s movement and boundary detection status.
•	BLE Car Control:
-	Establishes and maintains a BLE connection with the car using BTLEHandler.
-	Processes real-time movement data from the tracking module and sends appropriate speed and steering commands to the car.
-	Ensures continuous data transmission to the car to maintain smooth operation and avoid latency issues.
•	Multithreading and Real-Time Processing:
-	Uses multithreading to handle tracking, boundary detection, and BLE communication simultaneously.
-	Prevents UI blocking by running computationally intensive tasks (e.g., image processing, car control) in separate threads.
Functionality Summary: 
The main.py module acts as the orchestrator of the BLE car simulation, integrating tracking, user controls, and BLE communication. It enables real-time position tracking, vision-based guidance, and an interactive user interface for controlling the car efficiently. By leveraging multithreading and queue-based data handling, it ensures smooth and responsive operation in an autonomous or user-driven setting.

# CarSteering Package
The CarSteering package provides functionality for controlling a Bluetooth Low Energy (BLE) car via simple communication commands. It includes tools to connect to the car's BLE device, send control commands for steering, speed, and other parameters, and manage the car's movement. The package contains two main Python scripts:
•	BTLEHandler.py: Handles BLE device communication and sends commands for controlling the car's speed and direction.
•	client_new.py: Likely serves as the interface for using the BTLEHandler class and interacting with the car's controls

1.	BTLEHandler.py
Overview
The BTLEHandler.py script manages the communication with a Bluetooth Low Energy (BLE) car. It provides methods for connecting to the car’s BLE device, sending control commands (speed, direction), and controlling the car’s movement. The script uses the simplepyble library for BLE device management and communication.
Key Components:
-	The BTLEHandler class provides methods to connect to the BLE device, set control parameters, and send movement commands. It includes functionality to adjust speed, steering, and other controls, and sends these commands to the BLE device.
Attributes:
•	DEVICE_MAC: The MAC address of the target BLE car device.
•	DEVICE_CHARACTERISTIC: The UUID for the device's characteristic used for communication.
•	DEVICE_HANDLE: The handle for the device connection.
•	DEVICE_IDENTIFIER: The identifier used for generating commands.
•	Control: A list that holds speed, drift, and steering values.
•	CONTINUE_SENDING: A flag to control whether the system should keep sending commands continuously.
•	SET_LIGHT_VALUE, SET_CHECKSUM, and others: Predefined hexadecimal values used in the command structure.
Methods:
•	__init__(self):
-	Initializes the BTLEHandler with default values for device attributes, control settings, and command parameters.
•	connect(self):
-	Scans for nearby BLE devices and connects to the device with the specified MAC address. It retrieves and prints all available services and characteristics of the device, and establishes a connection.
•	twoDigitHex(self, number):
-	Converts a given integer into a two-digit hexadecimal string.
•	setSpeed(self, speed):
-	Sets the car's speed to the specified value (0-254). Ensures the value is within range.
•	setReverseSpeed(self, speed):
-	Sets the reverse speed by subtracting the specified value from 255.
•	increaseSpeed(self):
-	Increases the car’s speed by 1, within the valid range of 0 to 255.
•	decreaseSpeed(self):
-	Decreases the car’s speed by 1, within the valid range of 0 to 255.
•	driveRight(self, rightValue):
-	Adjusts the control parameters to turn the car to the right by modifying the right wheel value.
•	driveLeft(self, leftValue):
-	Adjusts the control parameters to turn the car to the left by modifying the left wheel value.
•	setContinueSending(self, var):
-	Sets whether the commands should continue to be sent endlessly or stop after a set number of iterations.
•	generateAndSend(self):
-	Generates the command in hexadecimal format based on the current control parameters and sends it to the connected BLE device.
•	generateCommand(self):
-	Constructs a command string by concatenating various control values and device identifiers, then converts it to a hexadecimal command.
Functionality Summary: 
The BTLEHandler.py script is designed to manage the communication between a controlling system (such as a PC or mobile device) and a BLE-controlled car. It allows setting the car's speed, steering, and other parameters, and then sends these commands via Bluetooth. The class provides methods to adjust speed, direction, and the continuous sending of commands, allowing for real-time control over the car's movement.
2.	client_new.py
Overview
The client_new.py script controls a DR!FT car using inputs from a gamepad and sends data to the car using Bluetooth Low Energy (BLE). The script has a main thread that runs a GUI using Tkinter, two additional threads for handling input and sending data, and controls for adjusting the car’s speed, direction, and lights.
Key Components
•	Inputs Polling Thread (inputs_poll):
-	Polls the gamepad for button presses and joystick movements.
-	Converts these inputs into control data (light, speed, right/left turn values) and places the data into a queue.
•	Sending Data Thread (send_data):
-	Continuously sends control data to the car using the DeviceHandler.
•	Car Controller (car_controller):
-	Handles the main logic of processing the control data and sending commands to the car for controlling lights, speed, and turning.
-	Controls the speed and direction by adjusting values based on input and updates the progress bars for visual feedback on the GUI.
•	BTLE Handler (DeviceHandler):
-	Handles the connection to the DR!FT car and sends commands (such as light control and movement commands) via Bluetooth Low Energy (BLE).
-	The performConnect method connects to the car, and other methods (performlightOn, performlightOff, etc.) control specific actions like lights and movement.
Methods
•	inputs_poll(queue):
-	Polls the gamepad for events like button presses and joystick movements.
-	Converts the events into a control data list that includes light control, speed, and right/left turn values.
-	Adds control data to a queue for the main thread to process.
•	send_data(handler):
-	Periodically calls generateAndSend from BTLEHandler to send data to the car.
-	Runs continuously in a separate thread.
•	car_controller(DeviceHandler, queue_input):
-	Processes the control data from the input thread, adjusting speed, lights, and turning.
-	Updates the progress bars in the GUI to reflect the current values of speed and turning.
•	performConnect():
-	Establishes a connection to the DR!FT car via Bluetooth.
-	Notifies the user if the connection is successful or not.
•	PercentageToNumber(percent):
-	Converts a percentage value into a number suitable for controlling the car.
•	performlightOn(blehandler):
-	Sends a command to the car to turn the lights on.
•	performlightOff(blehandler):
-	Sends a command to the car to turn the lights off.
•	performStop(blehandler):
-	Sends a stop command to the car, setting speed and direction to zero and turning off the lights.
•	car_connect(DeviceHandler, queue_input, mythr_2):
-	Handles the connection to the car and starts the sending thread (mythr_2) once the connection is successful.
-	Calls the car_controller method to control the car after the connection is established.
GUI Components
•	Tkinter GUI:
-	The script uses Tkinter to create a window with buttons and progress bars.
-	The progress bars (speed_bar, left_bar, right_bar) provide visual feedback on the car's speed and turning values.
-	A button labeled "center" connects to the car when clicked, initializing the car connection and starting the control process.
•	Progress Bars:
-	speed_bar: Represents the current speed of the car (forward or reverse).
-	left_bar: Represents the left turn value.
-	right_bar: Represents the right turn value.
Functionality Summary
The client_new.py script controls a DR!FT car by reading inputs from a gamepad and sending commands to the car via Bluetooth. The script runs in three threads: one for reading inputs, one for sending data, and one for controlling the car's behavior. It also provides a GUI that displays the current speed and turn values, giving the user real-time feedback on the car's state. The user can control the car's speed, direction, and lights, and the car's movements are reflected on the GUI through progress bars.

# Simulation Package
The Simulation package simulates the movement of an autonomous car on a track, utilizing pygame for visualization and boundary detection. The package includes a simulation.py script, configuration file, and image assets for the car and track. The car's movement is based on simple physics and boundary detection via rays.
1.	simulation.py
Overview
The simulation.py script simulates an autonomous car moving on a track by detecting boundaries using rays and adjusting its direction accordingly. The car moves based on information from the track and uses an image of the track and car to display the simulation.
Key Components
	Car Class:
-	The Car class models the car’s movement, direction, and decision-making process. It also tracks obstacles on the track using rays that detect boundaries.
•	Methods:
o	update_position(): Updates the car’s position based on its speed and direction.
o	update_rays(): Updates the position of rays based on the car's current location and direction to detect boundaries.
o	update_guidance(): Adjusts the car’s direction using the results from the rays' boundary detection.
o	evasive_action(): Alters the car's direction to avoid colliding with track boundaries.
o	find_min_max_rays(): Finds the rays with the minimum and maximum distance to the boundary.
o	decision_counter_check(): Ensures the car periodically changes direction to avoid getting stuck.
	Ray Class:
-	The Ray class simulates a ray cast from the car in multiple directions to detect boundaries (black pixels) on the track.
•	Methods:
o	update(): Updates the ray’s position and checks for boundaries.
o	update_direction(): Adjusts the direction of the ray based on the car’s angle.
o	update_terminus(): Calculates the ray's terminus point, detecting boundaries in the track image.
Boundary Detection:
-	The car uses rays cast at three angles (-60°, 0°, +60°) to detect the nearest boundary on the track. If the car gets too close to a boundary, it will adjust its direction.
Main Loop:
-	The main() function initializes the game window, loads the track and car images, and controls the simulation loop where the car's position is updated in real-time. The car is displayed with updated rays, showing its proximity to boundaries.
Functions
•	resize_image_to_fit(image, max_width, max_height):
-	Resizes the track and car images to fit within the specified screen dimensions while maintaining their aspect ratio.
•	find_white_dot_position(track):
-	Finds the position of the white dot on the track (used as the car’s starting point). If not found, a default position is used.
•	pygame.transform.scale():
-	Scales the images for the track and car to appropriate sizes based on the screen and car dimensions.
•	pygame.draw.line():
-	Draws the rays that represent the car's boundary detection.
Functionality Summary:
The simulation.py script simulates an autonomous car's movement on a track using boundary detection with rays that check for nearby obstacles. The car adjusts its direction based on the proximity of the track's boundary, avoiding collisions by altering its steering. It also includes a decision-making system that occasionally changes the car's direction randomly. The simulation is rendered using pygame, displaying the car's movement, boundary detection rays, and the track on the screen.
2.	config.txt
Overview
The config.txt file is used to configure parameters for the NEAT (NeuroEvolution of Augmenting Topologies) algorithm. It defines settings for fitness evaluation, genome structure, and the evolutionary process.
•	[NEAT]: Specifies global NEAT parameters such as fitness criteria, population size, and reset behavior on extinction.
•	[DefaultGenome]: Configures the properties of the neural network's genome, including node activation functions, bias settings, and connection behavior.
•	[DefaultSpeciesSet]: Defines species compatibility and thresholds for forming species in the population.
•	[DefaultStagnation]: Specifies conditions for stagnation, including the maximum number of generations without progress and species elitism.
•	[DefaultReproduction]: Configures the reproduction process, including elitism and survival thresholds for offspring.


# Tracking Package
The Tracking package provides core functionality for real-time object tracking using the GOTURN (Generic Object Tracking Using Regression Networks) model. It integrates computer vision techniques to track moving objects via webcam input. The package enables the selection of objects for tracking, calculates their movement vector, and visualizes the tracking process with the use of bounding boxes and movement arrows.
This package contains the following Python file:
-	testgoturn.py: Implements object tracking using the GOTURN tracker, allowing users to select an object and monitor its movement over time. It calculates and displays the movement vector, including speed and direction, and provides a real-time visual output.
1.	testgoturn.py
Overview
The testgoturn.py script is responsible for implementing object tracking using the GOTURN model. It allows the user to select an object from the webcam feed, initializes the tracker, and updates the tracking in real-time. The script calculates the object's position, movement vector, and displays relevant information on the frame, including the position, vector magnitude, and angle.
Key Components
•	Initialization:
-	Initializes the GOTURN tracker using OpenCV's cv2.TrackerGOTURN_create() method.
-	Opens the webcam for capturing frames.
-	Provides the user with the option to select the object they want to track.
•	Object Selection:
-	The user is prompted to select a Region of Interest (ROI) for the object to track via the OpenCV cv2.selectROI() method.
•	Tracking Loop:
-	The script reads frames from the webcam, updates the tracker, and draws a bounding box around the tracked object.
-	It continuously calculates the midpoint of the bounding box, storing the last 10 midpoints.
-	Calculates the movement vector (both magnitude and angle) if at least two midpoints are available.
-	Displays movement information (speed and direction) on the frame.
•	Movement Vector Calculation:
-	The script calculates the change in position (dx, dy) between the last two midpoints, which is used to compute the movement vector.
-	The magnitude of the vector is calculated using the Pythagorean theorem, and the angle is determined using atan2() to represent the direction of movement.
Key Functions
•	tracker.init(frame, bbox):
-	Initializes the GOTURN tracker with the selected region of interest (ROI) and the first frame.
•	tracker.update(frame):
-	Updates the tracking state and returns the bounding box of the tracked object.
•	cv2.selectROI():
-	Prompts the user to select the object to track by drawing a bounding box around it in the first frame.
•	cv2.arrowedLine():
-	Draws an arrow indicating the predicted movement vector of the tracked object.
•	cv2.putText():
-	Displays relevant tracking information (position, movement vector) on the frame.
Functionality Summary
The testgoturn.py script provides a real-time object tracking system using the GOTURN tracker. It allows the user to select an object, tracks its movement, calculates the movement vector, and displays this information on the webcam feed. The script continuously updates the position and movement information while providing a visual representation of the object's movement over time.
Vidgear Example Stream Package
The Vidgear Example Stream package demonstrates how to set up a simple streaming client-server architecture using the Vidgear library. It allows for the transmission of video frames over the network from a server to a client, where the client processes and displays the received frames. This package includes two main Python scripts: client.py and server.py.
This package contains the following Python files:
•	client.py: Implements the client-side functionality, receiving video frames over the network and displaying them using OpenCV.
•	server.py: Implements the server-side functionality, sending video frames to the client.
1.	client.py
Overview
The client.py script is responsible for receiving video frames from a network stream and displaying them in real-time using OpenCV. It uses the NetGear class from the Vidgear library to establish a connection to the server, receive frames, and show the output on a window. The client can be stopped by pressing the 'q' key.
Key Components
•	Initialization:
-	Configures the connection parameters for the client, including the server’s IP address, port, and protocol. It also sets the flags for copying and tracking options, and logging for debugging.
•	NetGear Client:
-	The NetGear class is used to create a network client, which connects to the server at the specified IP address and port. The client is configured to receive frames and logs network activity.
•	Frame Reception:
-	The client continuously receives frames from the server in a loop. If no frame is received (i.e., None), the loop terminates.
•	Frame Display:
-	The received frames are displayed using OpenCV’s cv2.imshow() function. The user can press the 'q' key to exit the loop and close the client.
•	Client Shutdown:
-	After the loop ends, the output window is closed, and the connection to the server is safely terminated using the client.close() method.
Key Functions
•	client.recv():
-	Receives a frame from the server. If no frame is received, it returns None.
•	cv2.imshow():
-	Displays the received frame in an OpenCV window.
•	cv2.waitKey():
-	Captures user key input. If the 'q' key is pressed, it exits the loop and closes the window.
•	client.close():
-	Safely closes the connection to the server once the streaming is complete.
Functionality Summary
The client.py script establishes a connection to a remote server using Vidgear’s NetGear class, continuously receives video frames, and displays them in real-time. It allows users to view the video stream and exit the application when the 'q' key is pressed. The script provides a basic example of receiving video streams in a networked client-server setup.
2.	server.py
Overview
The server.py script captures video frames from a webcam or any suitable video stream and sends these frames to a client over a network using the NetGear class from the Vidgear library. The script continues to stream video until interrupted by the user (e.g., via keyboard interrupt). It also manages the connection and logging functionality for the server.
Key Components
•	Video Stream Initialization:
-	The script opens a video stream using OpenCV’s cv2.VideoCapture() method, which connects to the default webcam (index 0). This can be modified to use other video sources as needed.
•	NetGear Server:
-	The NetGear class is used to create a server that sends frames to a connected client. The server is configured to send the video stream to a specific IP address and port over TCP, with logging enabled for debugging purposes.
•	Frame Capture and Transmission:
-	The script continuously captures frames from the webcam or video stream using stream.read(). If a frame is successfully captured, it is sent to the client via the server.send() method.
•	Error Handling:
-	If the frame is not grabbed (i.e., the video stream is unavailable), the loop exits. The server also handles keyboard interruptions to safely terminate the stream.
•	Server Shutdown:
-	After the loop ends, the video stream is released, and the server connection is safely closed using the server.close() method.
Key Functions
•	stream.read():
-	Captures a frame from the video stream. It returns a tuple, where grabbed indicates whether the frame was successfully read, and frame contains the captured video frame.
•	server.send(frame):
-	Sends the captured video frame to the client over the network.
•	server.close():
-	Safely closes the server connection once the video stream has ended.
•	stream.release():
-	Releases the video capture object and closes the video stream.
Functionality Summary
The server.py script captures video frames from a webcam or other video source and streams them over the network to a client using the Vidgear NetGear library. The server continues streaming until a keyboard interrupt is received, after which it gracefully shuts down the video stream and connection. This script provides a basic example of setting up a video streaming server for a client-server architecture.



