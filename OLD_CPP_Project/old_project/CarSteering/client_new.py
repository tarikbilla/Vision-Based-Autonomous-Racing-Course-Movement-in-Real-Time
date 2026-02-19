from tkinter import *
from tkinter.ttk import *
import tkinter.ttk as ttk
from PIL import ImageTk, Image
from BTLEHandler import BTLEHandler
import inputs
import BTLEHandler as B
from queue import Queue, Full
import time
import threading

##One main and two threds, one for input and one for sending data to the car. Pass inputs to main thread using a queue. 

def inputs_poll(queue):
    #local_count = 0
    control_data = [0,0,0,0] # 0 light control, 1 Speed, 2 Right value, 3 left value
    while 1:
        #local_count= local_count+1
        events = inputs.get_gamepad()
        if events:
            for event in events:
                if(event.code == "BTN_WEST" and event.state == 1):
                    control_data[0] = 1
                elif(event.code == "BTN_SOUTH" and event.state == 1):
                    control_data[0] = 0
                elif (event.code == "BTN_EAST" and event.state == 1):
                    #control_data[0] = 0
                    control_data[1] = 0
                    control_data[2] = 0
                    control_data[3] = 0
                elif (event.code == "ABS_RZ"):
                    control_data[1] = int((event.state))
                elif (event.code == "ABS_Z"):
                    control_data[1] = -int((event.state))
                elif (event.code == "ABS_X"):
                    control_data[2] = int((event.state / 32767) * 255) if event.state > 0 else 0
                    control_data[3] = int((event.state / -32768) * 255) if event.state < 0 else 0
        try:
            queue.put_nowait(control_data)
        except Full:
            # Remove the oldest item and try again
            queue.get_nowait()  # Discard the oldest item
            queue.put_nowait(control_data)  # Retry adding the new item
        global stop_flag
        if stop_flag:
            return

def send_data(handler):
    while 1:
        handler.generateAndSend()
        global stop_flag
        if stop_flag:
            return
        time.sleep(0.005)

def car_controller(DeviceHandler,queue_input):
    #B.DeviceHandler = BTLEHandler()
    B.DeviceHandler = DeviceHandler
    #performConnect()
    aria = 0
    #while True:
    #events = inputs.get_key()
    #events = inputs.get_gamepad()
    b = 200
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
            #B.DeviceHandler.setSpeed((acceleration))
        else:
            B.DeviceHandler.setReverseSpeed(0)
        
        speed_bar['value'] = acceleration

        right_turn = events[2]
        left_turn = abs(events[3])
        if (right_turn>100):
            right_turn = 100
        if (left_turn>100):
            left_turn = 100

        uno_x = right_turn  
        right_bar['value'] = uno_x

        uno_x = left_turn 
        left_bar['value'] = uno_x

        if right_turn > 0:
            B.DeviceHandler.driveRight(right_turn)
            #B.DeviceHandler.driveLeft(0)
        elif left_turn > 0:
            B.DeviceHandler.driveLeft(left_turn)
            #B.DeviceHandler.driveRight(0)
        else:
            B.DeviceHandler.driveRight(0)
            #B.DeviceHandler.driveLeft(0)
        

    main_screen.after(1,func= lambda: car_controller(DeviceHandler,queue_input))

def performConnect():
    global Characteristics

    Characteristics = B.DeviceHandler.connect()
    if (Characteristics):
        print(" => Connected to DR!FT Device!")
        # function for video streaming
        IsConnected = 1
    else:
        print(" => Failed to Connect!")


def PercentageToNumber(percent):
    return abs(round((255 * percent) / 100))

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


def performStop(blehandler):
        #if (IsConnected != 0):
            # Set Speed and Direction 0
            blehandler.Control[2] = 0
            blehandler.Control[0] = 0
            # Turn off Light and set Checksum
            blehandler.SET_LIGHT_VALUE = '0000'
            blehandler.SET_CHECKSUM = '4e'
            # Run Command
            #recentCommand = blehandler.generateAndSend()
            #print("Stop Driving\n")
            #print("Executed Command:" + recentCommand + '\n')

def car_connect(DeviceHandler,queue_input,mythr_2):
    Characteristics = DeviceHandler.connect()
    if (Characteristics):
        print(" => Connected to DR!FT Device!")
        mythr_2.start()
        # function for video streaming
        IsConnected = 1
        main_screen.after(10,func= lambda: car_controller(DeviceHandler,queue_input))
    else:
        print(" => Failed to Connect!")
    


##MAIN THREAD
if __name__ == '__main__':
    DeviceHandler = BTLEHandler()
    speed_hex = '00'
    original_speed = 0
    original_direction = 0
    direction_hex = '00'
    reverseFlag = 0
    IsConnected = 0
    stop_flag = False

    ##Input thread initialization
    queue_input = Queue(maxsize=20)
    mythr_1 = threading.Thread(target=inputs_poll,args=(queue_input,))
    mythr_1.start()

    ##Sending thread initialization
    mythr_2 = threading.Thread(target=send_data,args=(DeviceHandler,))


    main_screen = Tk()
    main_screen.geometry('956x719')
    main_screen.title("Welcome to DR!FT cars")
    #image1 = PhotoImage(file=r'.\photos\fondo.png')
    image1 = PhotoImage(file='Python/photo/fondo.png')
    label_for_image = Label(main_screen, image=image1)
    label_for_image.place(x=0, y=0)


    app = Frame(width="390", height="100")
    app.place(x=230, y=200)
    # Create a label in the frame

    lmain = Label(app)
    lmain.grid()


    #photo_center = PhotoImage(file=r".\photos\start_engine.png")
    photo_center = PhotoImage(file="Python/photo/Startengine.png")
    Button(main_screen, text="center", image=photo_center, command=lambda: car_connect(DeviceHandler,queue_input,mythr_2)).place(x=700, y=550)


    s = ttk.Style()
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
        mythr_1.join()
        mythr_2.join()