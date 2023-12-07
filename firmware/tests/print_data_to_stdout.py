import serial

with serial.Serial('/tmp/ttyUSB0', baudrate=115200, timeout=None) as ser:
    terminator = b'remove printed data from the device? (y/N): '

    ser.write(b"get data\r\n")
    msg = ser.read_until(terminator)
    msg = msg.removesuffix(terminator).decode('utf-8')
    print(msg)
