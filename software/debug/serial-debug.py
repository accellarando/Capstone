import serial
import time

arduino = serial.Serial(port='/dev/ttyACM0', baudrate= 115200, timeout= 1)

def write_read(x):
    arduino.write(bytes(x, 'utf-8'))
    time.sleep(0.05)
    data = arduino.readline()
    print(data)
    return data
    
while True:
    num = input("Enter a num: ")
    value = write_read(num)
    print(value.decode().strip())
