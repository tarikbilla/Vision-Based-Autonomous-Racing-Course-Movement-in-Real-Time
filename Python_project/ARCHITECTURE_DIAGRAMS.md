# System Architecture & Flow Diagrams

## System Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         RC AUTONOMY SYSTEM v2.0                         │
└─────────────────────────────────────────────────────────────────────────┘

                              ORCHESTRATOR
                         (Main Control Hub)
                                 │
                ┌────────────────┼────────────────┐
                │                │                │
         ┌──────▼────────┐  ┌────▼──────────┐  ┌─▼──────────────┐
         │   CAMERA      │  │   TRACKER    │  │  BOUNDARY      │
         │   LOOP        │  │   LOOP       │  │  DETECTOR      │
         ├───────────────┤  ├──────────────┤  ├────────────────┤
         │ • Capture     │  │ • Update     │  │ • Road Edges   │
         │ • 30 FPS      │  │ • Position   │  │ • Rays         │
         │ • Queue       │  │ • Movement   │  │ • Steering     │
         └──────┬────────┘  └────┬─────────┘  └────┬───────────┘
                │                │                 │
                └────────────────┼─────────────────┘
                                 │
                            ┌────▼─────────┐
                            │   RENDER     │
                            │   THREAD     │
                            ├──────────────┤
                            │ • Live View  │
                            │ • Analysis   │
                            │ • Edges      │
                            │ • Centerline │
                            └────┬─────────┘
                                 │
                            ┌────▼──────────┐
                            │  BLE LOOP     │
                            ├───────────────┤
                            │ • Send Cmds   │
                            │ • 200Hz Rate  │
                            │ • Car Ctrl    │
                            └───────────────┘
                                 │
                            ┌────▼──────────┐
                            │  PHYSICAL RC  │
                            │    CAR        │
                            └───────────────┘
```

## Detection Flow Diagram

```
                    CAMERA FRAME
                          │
              ┌───────────┴──────────┐
              ▼                      ▼
        ┌─────────────┐      ┌──────────────┐
        │ HSV Convert │      │ Color Mask   │
        │ (BGR→HSV)   │      │ (Red/White)  │
        └──────┬──────┘      └──────────────┘
               │                     │
               └──────────┬──────────┘
                          ▼
               ┌────────────────────┐
               │ Edge Detection     │
               │ (Road Boundaries)  │
               └────────┬───────────┘
                        │
          ┌─────────────┴──────────────┐
          │                            │
          ▼                            ▼
    ┌──────────────┐           ┌────────────────┐
    │ Edges Found? │           │ Edges Found?   │
    │    YES       │           │     NO         │
    └──────┬───────┘           └────────┬───────┘
           │                            │
           ▼                            ▼
    ┌─────────────────┐        ┌─────────────────┐
    │ Calculate:      │        │ Try Lane Detect │
    │ • Left Edge     │        │ (Red+White)     │
    │ • Right Edge    │        └────────┬────────┘
    │ • Centerline    │                 │
    └────────┬────────┘        ┌────────┴────────┐
             │                 │                 │
             │        ┌────────▼───────┐  ┌─────▼──────┐
             │        │ Lane Found?    │  │   Not      │
             │        │   YES          │  │  Found     │
             │        └─────┬──────────┘  └─────┬──────┘
             │              │                   │
             └──────────────┼───────────────────┘
                            │
                    ┌───────▼────────┐
                    │ Track Red Car  │
                    │ by Position    │
                    └────────┬───────┘
                             │
                    ┌────────▼─────────┐
                    │ Calculate Error  │
                    │ vs Centerline    │
                    └────────┬─────────┘
                             │
                    ┌────────▼──────────┐
                    │ Generate Steering │
                    │ Control Commands  │
                    └────────┬──────────┘
                             │
                    ┌────────▼─────────┐
                    │ Send to RC Car   │
                    │  via BLE         │
                    └──────────────────┘
```

## Tracking Mode Selection

```
USER PRESSES KEY
       │
    ┌──┴────────────────┐
    │                   │
    ▼'a'               ▼'s'
  AUTO MODE        MANUAL MODE
    │                   │
    ├─────────────┐     ├─────────────┐
    │             │     │             │
    ▼             │     ▼             │
Enable Color    │  Freeze Frame    │
Tracking        │  Show ROI Sel    │
    │             │     │             │
    ▼             │     ▼             │
Search for      │  User Selects   │
Road Edges      │  ROI (click&drag)│
    │             │     │             │
    ├─YES─┐       │     ├─Valid─┐     │
    │     │       │     │       │     │
    │     ▼       │     │       ▼     │
    │  Draw:     │     │  Init Tracker│
    │  • Edges   │     │     │        │
    │  • Center  │     │     ├─Success┤
    │  • Track   │     │     │ Start! │
    │             │     │     │        │
    │     │       │     │     ├─Fail   │
    │     │       │     │     │ Retry  │
    └─NO──┤       │     │     │ (×5)   │
        (Fall    │     │     │        │
         back)   │     │     ├─5 Fail─┤
             │   │     │     │        │
             └───┼─────┴─────┘        │
                 │                    │
           ┌─────▼──────┐             │
           │ Color Track │◄────────────┘
           │ Red Car     │
           │ Only        │
           └─────────────┘
```

## Real-Time Steering Control

```
DETECTED CAR POSITION: car_x = 320px (image width = 640px)
ROAD CENTER:          center_x = 300px
ERROR:                error = 320 - 300 = +20px (car to the right)
TOLERANCE:            ±20px

                      ┌──────────────────┐
                      │ ERROR ANALYSIS   │
                      ├──────────────────┤
                      │ Error = +20px    │
                      │ Within Tolerance │
                      └────────┬─────────┘
                               │
                    ┌──────────▼──────────┐
                    │ STEERING OUTPUT     │
                    ├─────────────────────┤
                    │ Left Turn:  0       │
                    │ Right Turn: 0       │
                    │ Speed: 10           │
                    │ Action: STRAIGHT    │
                    └─────────────────────┘


IF ERROR WAS +50px (CAR TOO FAR RIGHT):

                    ┌──────────────────┐
                    │ ERROR ANALYSIS   │
                    ├──────────────────┤
                    │ Error = +50px    │
                    │ > Tolerance      │
                    └────────┬─────────┘
                             │
                  ┌──────────▼──────────┐
                  │ STEERING OUTPUT     │
                  ├─────────────────────┤
                  │ Left Turn:  15      │ ← Turn LEFT
                  │ Right Turn: 0       │
                  │ Speed: 10           │
                  │ Action: STEER LEFT  │
                  └─────────────────────┘


IF ERROR WAS -100px (CAR TOO FAR LEFT):

                    ┌──────────────────┐
                    │ ERROR ANALYSIS   │
                    ├──────────────────┤
                    │ Error = -100px   │
                    │ >> Tolerance     │
                    └────────┬─────────┘
                             │
                  ┌──────────▼──────────┐
                  │ STEERING OUTPUT     │
                  ├─────────────────────┤
                  │ Left Turn:  0       │
                  │ Right Turn: 30      │ ← Max RIGHT
                  │ Speed: 5            │ ← Reduce speed
                  │ Action: STEER RIGHT │
                  └─────────────────────┘
```

## Live Display Output

```
When Color Tracking Enabled (Press 'a'):

    LIVE CAMERA WINDOW                    ANALYSIS WINDOW
    ┌──────────────────────────┐          ┌──────────────────────────┐
    │ Road Center: 320         │          │ Distance Rays            │
    │                          │          │                          │
    │  |      ║      ║      |  │          │  |      ║      ║      |  │
    │  |      ║      ║      |  │          │  |  150 ║ 180  ║  160 |  │
    │  |      ║ [car]║      |  │          │  |      ║ [car]║      |  │
    │  |      ║      ║      |  │          │  |      ║ ●    ║      |  │
    │  |      ║      ║      |  │          │  |______║______║______|  │
    │ Green = edges             │          │ Yellow = center        │
    │ Blue = centerline          │          │ Red = car bounding box  │
    └──────────────────────────┘          └──────────────────────────┘

Console Output (Every 30 frames ~1sec):
[1] Center: (320, 240) | Speed: 10 | Left: 0 | Right: 0 | Rays: L=150 C=180 R=160
[2] Center: (325, 240) | Speed: 10 | Left: 5 | Right: 0 | Rays: L=148 C=175 R=162
[3] Center: (318, 240) | Speed: 10 | Left: 0 | Right: 2 | Rays: L=152 C=182 R=158
```

## Component Interaction Sequence

```
Time ──────────────────────────────────────────────────────────────────►

CAMERA_LOOP:      Read──Read──Read──Read──Read──Read──Read
                   │    │    │    │    │    │    │
TRACKING_LOOP:         Process──Process──Process──Process
                       │    │    │    │
UI_LOOP (Render):      Show──Show──Show──Show
                           │    │    │
BLE_LOOP:                 Send──Send──Send  (200Hz)
                              │    │    │

Legend:
  ─ = waiting
  │ = processing/communicating
  ─► = time progression
```

## Decision Tree: Which Tracking Mode to Use

```
                    START SYSTEM
                         │
                    Press Key?
                    /   │   \
                   a    s    q
                  /     │     \
                 │      │      └─ QUIT
                 │      │
    ┌────────────┘      └────────────────┐
    │                                    │
    ▼                              ┌─────▼──────┐
 AUTO MODE                         │ Manual ROI │
    │                              │ Selection  │
    ├─ Good lighting?              └─────┬──────┘
    │                                    │
    ├─ Clear road edges?           Can you see
    │                              the car clearly?
    │                                    │
    ├─ Red car on track?                 ├─ Yes
    │                                    │
    └─ Working well?         ┌───────────┘
       │                     │
       ├─ Yes                ▼
       │  SUCCESS!       Drag to select
       │                 small region
       │                 around car
       └─ No            │
          Try 's'        ├─ Good ROI?
          (manual)       │  ├─ Yes
                         │  │ Tracker init
                         │  │ ├─ Success
                         │  │ │ START!
                         │  │ └─ Fail
                         │  │   (Retry)
                         │  │
                         │  └─ No (too small)
                         │     Try again
                         │
                         └─ Cancelled
                            Press 'a'
                            for auto
```

---

This visual architecture shows how all components work together in real-time to track the RC car and guide it along the road using detected edge markers.
