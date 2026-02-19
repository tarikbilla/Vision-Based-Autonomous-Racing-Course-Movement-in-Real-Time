# Vision-Based Autonomous RC Car Control System

## Project Overview
The **Vision-Based Autonomous RC Car Control System** is a computer vision–driven project designed to control an RC car autonomously using real-time video input. The system integrates an external camera, Raspberry Pi, and Bluetooth communication to transition from manual control to fully autonomous navigation. The project focuses on real-time performance, accurate path detection, and robust decision-making using C++.

---

## Objectives
- Develop a complete control pipeline for an RC car  
- Enable real-time vision-based navigation using C++  
- Implement autonomous steering and motion control  
- Optimize latency and system reliability  

---

## System Architecture
- **External Camera** – Captures real-time video feed  
- **PC** – Runs C++-based image processing and control logic  
- **Raspberry Pi** – Receives control commands and interfaces with the RC car  
- **Bluetooth Module** – Wireless communication between PC and RC car  

---

## Project Workflow

### Phase 1: Manual Control Setup
- Setup camera, PC, Raspberry Pi, and Bluetooth communication  
- Implementation of basic control commands:
  - Start  
  - Stop  
  - Left  
  - Right  
- Validation of the complete control and communication pipeline  

### Phase 2: Vision Processing
- Real-time video capture from an external camera  
- Image preprocessing using OpenCV in C++  
- Detection of the RC car and track/path  

### Phase 3: Autonomous Control
- Decision-making logic based on vision output  
- Automatic generation of steering and speed commands  
- Transmission of control commands to the RC car  

### Phase 4: Machine Learning Integration
- Integration of ML-based object detection models  
- Improved detection accuracy and robustness  
- Handling varying lighting and track conditions  

### Phase 5: Optimization & Testing
- Real-time performance and latency optimization  
- Testing on multiple track layouts  
- System evaluation and fine-tuning  

---

## Technologies Used
- **C++**  
- **OpenCV**  
- **Machine Learning / Computer Vision Algorithms**  
- **Raspberry Pi**  
- **Bluetooth Communication**  

---

## Features
- Manual and autonomous driving modes  
- Real-time image processing in C++  
- Vision-based path detection  
- Autonomous control logic  
- Modular and scalable architecture  

---

## Testing & Results
- Tested on different track configurations  
- Evaluated system responsiveness and accuracy  
- Demonstrated stable autonomous navigation in controlled environments  

---

## Future Improvements
- Deep learning–based lane and object detection  
- Full edge processing on Raspberry Pi  
- Obstacle avoidance and dynamic path planning  
- Sensor fusion with ultrasonic or LiDAR sensors  

---

## License
This project is intended for educational and research purposes.
