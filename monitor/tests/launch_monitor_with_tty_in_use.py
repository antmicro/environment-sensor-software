import serial
import subprocess

if __name__ == '__main__':
    with serial.Serial('/tmp/ttyUSB0', exclusive=True) as ser:
        print(subprocess.check_output(["python ../antenvsens_monitor.py -g -v -o ./tmp"], shell=True, text=True))