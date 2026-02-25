import simplepyble

#
# Module for connecting to the BLE Device and sending Steering commands
# Developed by: Marvin Mkrtschjan 
# 2019, THM
# Edited and changed by: Varshini, Abhyshek and Michel
# 2025, THM
#
# inputs: Control[0] Speed, Control[1] Drift(Turns the chassie), Control[2] Steering

class BTLEHandler:
    def __init__(self):
        self.DEVICE_CHARACTERISTIC = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'
        # gray car = 'f9:af:3c:e2:d2:f5'
        # red car = 'ed:5c:23:84:48:8d'
        self.DEVICE_MAC = 'f9:af:3c:e2:d2:f5'
        self.DEVICE_HANDLE = '000d'
        self.DEVICE_IDENTIFIER = 'bf0a00082800'
        self.ZERO_HEX = '00'
        self.SET_ALTERNATING_SPEED_VALUE = self.ZERO_HEX
        self.SET_ALTERNATING_VALUE_LEFT_WHEEL = self.ZERO_HEX
        self.SET_ALTERNATING_VALUE_RIGHT_WHEEL = self.ZERO_HEX
        self.SET_CHECKSUM = self.ZERO_HEX

        # Default is endless sending
        self.CONTINUE_SENDING = 1

        self.SET_LIGHT_VALUE = '0000'

        # Speed, Left Wheel, Right Wheel
        self.Control = [0, 0, 0]

    def connect(self):
        global Characteristics
        global peripheral
        choice = 0
        print("Connecting to Device:", self.DEVICE_MAC)
        #p = btle.Peripheral(self.DEVICE_MAC, btle.ADDR_TYPE_RANDOM)
        #p.setDelegate(MyDelegate())
        adapters = simplepyble.Adapter.get_adapters()
        adapter = adapters[0]
        adapter.scan_for(5000)
        peripherals = adapter.scan_get_results()
        for i, peripheral in enumerate(peripherals):
            #print(f"{i}: {peripheral.identifier()} ,[{peripheral.address()}]")
            if (str(peripheral.address()) ==str(self.DEVICE_MAC)):
                choice = i
                break
        peripheral = peripherals[choice]
        print(f"Connecting to: {peripheral.identifier()} [{peripheral.address()}]")
        peripheral.connect()
        print("Successfully connected, listing services...")
        services = peripheral.services()
        service_characteristic_pair = []
        for service in services:
            print(f"Service: {service.uuid()}")
            for characteristic in service.characteristics():
                print(f"    Characteristic: {characteristic.uuid()}")
                service_characteristic_pair.append((service.uuid(), characteristic.uuid()))
                #service_characteristic_pair.append((characteristic.uuid()))

        for i, (service_uuid, characteristic) in enumerate(service_characteristic_pair):
            print(f"{i}: {service_uuid} {characteristic}")
            if (str(characteristic) == str(self.DEVICE_CHARACTERISTIC)):
                choice = i

        ##Old code
        Characteristics = service_characteristic_pair
        print("Connected!", self.DEVICE_MAC)
        return Characteristics

    def twoDigitHex(self, number):
        # Convert Digit to Hex
        #return '%02x' % number
        return "{0:04x}".format(number)

    def setSpeed(self, speed):
        if speed < 255:
            self.Control[0] = speed

    def setReverseSpeed(self, speed):
        if speed < 255:
            self.Control[0] = 255 - speed

    def increaseSpeed(self):
        # Increase Speed value
        if self.Control[0] < 255:
            self.Control[0] += 1

    def decreaseSpeed(self):
        # Decrease Speed value
        if self.Control[0] > 0:
            self.Control[0] -= 1

    def driveRight(self, rightValue):
        # Increase value for Right, Decrease value for Left
        #print("Set New Right Value: " + str(rightValue))
        if rightValue < 255:
            self.Control[2] = rightValue

    def driveLeft(self, leftValue):
        # Increase value for Left, Decrease value for Right
        #print("Set New Left Value: " + str(leftValue))
        if leftValue < 255:
            self.Control[2] = 255 - leftValue

    def generateAndSend(self):
        comm = self.generateCommand()
        #Characteristics[4].write(bytes.fromhex(comm))
        service_uuid, characteristic_uuid = Characteristics[4]
        peripheral.write_request(service_uuid, characteristic_uuid, bytes.fromhex(comm))
        return comm

    def generateCommand(self):
        # Generate the HEX Command based on the determined patterns
        # Command = "".join([self.DEVICE_IDENTIFIER, self.SET_ALTERNATING_SPEED_VALUE, self.twoDigitHex(self.Control[0]) \
        #                       , self.ZERO_HEX, self.ZERO_HEX \
        #                       , self.ZERO_HEX, self.twoDigitHex(self.Control[2]) \
        #                       , self.SET_LIGHT_VALUE, self.SET_CHECKSUM])
        Command = "".join([self.DEVICE_IDENTIFIER,self.twoDigitHex(self.Control[0]),self.twoDigitHex(self.Control[1]),self.twoDigitHex(self.Control[2]),self.SET_LIGHT_VALUE,self.SET_CHECKSUM])


        return Command