# this script simulates disconnection of envsens during reading by replacing .readline() with always throwing version
# this is required because disconnecting uart connected using Renode's pty connector doesn't behave like physically disconnecting device
# and pyserial waits indefinitely on tty instead of throwing

import sys
sys.path.append('..')
import antenvsens_monitor
import serial
import linecache, os


def line_tracer(frame, event, arg):
    if event != 'line':
        return line_tracer
    
    line = linecache.getline(frame.f_code.co_filename, frame.f_lineno)
    if 'sensor.serial.readline()' in line:
        old_readline = frame.f_locals['sensor'].serial.readline

        def new_readline(*args):
            frame.f_locals['sensor'].serial.readline = old_readline
            raise serial.SerialException("test")

        frame.f_locals['sensor'].serial.readline = new_readline
    return line_tracer

def call_tracer(frame, event, arg):
    if frame.f_code.co_name == 'get_data' and 'antenvsens_monitor.py' in frame.f_code.co_filename and event == 'call':
        return line_tracer
    return

if __name__ == '__main__':
    sys.settrace(call_tracer)
    sys.argv = ['', '-g', '-v', '-o', './tmp']
    antenvsens_monitor.main()
