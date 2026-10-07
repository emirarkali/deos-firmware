import serial
import time

ser = serial.Serial("/dev/ttyUSB0", 115200)

while True:
    data = b"DEOS_TEST_123\r\n"
    ser.write(data)
    ser.flush()
    print("TX:", data)
    time.sleep(1)