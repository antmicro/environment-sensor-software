from dataclasses import dataclass
from datetime import datetime
from typing import Callable

import os
import serial
import argparse
import subprocess

baudrate = 115200
timeout = 3
verbose = False

def log_verbose(msg: str):
    if verbose:
        print(msg)

@dataclass
class Sensor:
    port: str
    name: str
    serial: serial.Serial


@dataclass
class EnvironmentalData:
    bme_temp: float
    bme_pressure: float
    bme_humidity: float
    sht_temp: float
    sht_humidity: float

class DevInfo:
    FTDI_VENDOR_ID = '0403'
    # maps ID_MODEL_ID to the trailing part of ID_PATH
    DEVICE_PORTS = {
        '6011' : '.2', #rev1.0.1
        '6001' : '.0'  #rev1.1.0
    }

    def __init__(self, tty_path):
        udevadm_info = self.get_dev_udevadm_info(tty_path)
        self.vendor_id = self.get_property(udevadm_info, "ID_VENDOR_ID")
        self.model_id = self.get_property(udevadm_info, "ID_MODEL_ID")
        self.path = self.get_property(udevadm_info, "ID_PATH")

    def get_dev_udevadm_info(self, tty_path):
        try:
            return subprocess.check_output(f"udevadm info --name={tty_path}", shell=True, text=True)
        except:
            return ""

    def get_property(self, udevadm_info, prop_name):
        for line in udevadm_info.splitlines():
            if prop_name in line:
                fields = line.split("=")
                if len(fields) > 1:
                    return fields[1]
        return ""
    
    def is_possible_env_sens(self):
        if "" in [self.vendor_id, self.model_id, self.path]:
            return True
        if self.vendor_id == self.FTDI_VENDOR_ID and self.model_id in self.DEVICE_PORTS:
            env_sens_port = self.DEVICE_PORTS[self.model_id]
            if len(self.path) >= len(env_sens_port):
                return self.path[-len(env_sens_port):] == self.DEVICE_PORTS[self.model_id]
        return False


devices: list[Sensor] = []

def is_duplicate(sensor_name: str):
    for sensor in devices:
        if sensor.name == sensor_name:
            return True, sensor.port
    return False, ""

def list_candidates():
    candidates = []
    for path, _, files in os.walk("/dev"):
        for f in files:
            if f.startswith("ttyUSB"):
                fp = os.path.join(path, f)
                if DevInfo(fp).is_possible_env_sens():
                    candidates.append(fp)
    return candidates

def scan_devices(allow_invalid_names: bool):
    print("Scanning devices...")

    for candidate in list_candidates():
        try:
            ser = serial.Serial(candidate, baudrate=baudrate, timeout=timeout, exclusive=True)
            ser.write(b"\r\n") # if script was previously killed board may still be waiting for data removal confirmation
            ser.readline()
            ser.write(b"get name\n")
            line = ser.readline()

            if line == b'': # there is no board on this tty
                ser.close()
                continue

            line = ser.readline()
            name = line.decode("utf-8").replace("\r\n", "").strip()

            if not allow_invalid_names:
                duplicate, port = is_duplicate(name)
                if name == '':
                    print(f"{candidate}: Name cannot be empty")
                    continue
                if duplicate:
                    print(f"{candidate}: Duplicated name ('{name}' already exists on {port})")
                    continue

            else:
                name = candidate.replace("/", "\\") + "\\" + name

            print(f"{name} found on {candidate}")

            devices.append(Sensor(candidate, name, ser))
        except serial.SerialException:
            print(f"serial exception on {candidate}")

def set_time():
    for sensor in devices:
        try:
            command = b"set time " + datetime.now().isoformat(timespec="seconds").encode()
            sensor.serial.write(command + b"\n")
            log_verbose(f"setting {sensor.name} time")
            echo = sensor.serial.readline()
            if echo != command + b"\r\n":
                log_verbose(f"time command echo missing")
            if sensor.serial.readline() != b"time set\r\n": # confirmation
                log_verbose("response missing")
        except serial.SerialException:
            print(f"serial exception on {sensor.name}")

def get_data(output_path: str, temp_str_gen: Callable[[EnvironmentalData], str], hum_str_gen: Callable[[EnvironmentalData], str], press: bool, separator: str):
    if len(devices) > 0:
        os.makedirs(output_path, exist_ok=True)

    for sensor in devices:

        try:
            data: list[bytes] = []

            command = b"get data"
            sensor.serial.write(command + b"\n")
            log_verbose(f"reading from {sensor.name}")
            line = sensor.serial.readline()
            if line != command + b"\r\n":
                log_verbose(f"read command echo missing")

            ack = False
            while line != b'':
                line = sensor.serial.readline()
                if line == b'remove printed data from the device? (y/N): ':
                    ack = True
                    break
                data.append(line)

            log_verbose(f"read {len(data)} entries")

            if not ack:
                log_verbose("confirmation dialog missing")
                continue

            filename = f"{output_path}/{sensor.name.replace(' ', '-')}"
            write_count = 0
            for entry in data:

                entry = entry.decode().replace("\r\n", "").split(",")
                try:
                    time_date = entry[0]
                    env_data = EnvironmentalData(*entry[1:])

                    
                    with open(filename , "a", encoding="utf-8") as f:
                        write_field = lambda field : f.write(field+separator)

                        write_field(f"{time_date}")
                        if temp_str_gen:
                            write_field(temp_str_gen(env_data))
                        if hum_str_gen:
                            write_field(hum_str_gen(env_data))
                        if press:
                            write_field(f"Pressure={env_data.bme_pressure}")
                        f.write("\n")
                        write_count += 1
                finally:
                    continue

            log_verbose(f"{write_count} entries have been written to {filename}")

            sensor.serial.write(b"y\n")
            log_verbose(f"sent confirmation")
            echo = sensor.serial.readline()
            if echo != b"y\r\n":
                log_verbose("confirmation echo missing")

        except serial.SerialException:
            log_verbose(f"serial exception on {sensor.name}")

def set_period(period: int):
    for sensor in devices:
        try:
            command = b"set period " + f"{period}".encode()
            sensor.serial.write(command + b"\n")
            log_verbose(f"setting {sensor.name} period")
            echo = sensor.serial.readline()
            if echo != command + b"\r\n":
                log_verbose(f"period command echo missing")
            if sensor.serial.readline() != b"period set\r\n": # confirmation
                log_verbose("response missing")
        except serial.SerialException:
            print(f"serial exception on {sensor.name}")

temp_str_gens = {
    'both': (lambda env_data : f"Temperature_BME={env_data.bme_temp} Temperature_SHT={env_data.sht_temp}"),
    'sht': (lambda env_data : f"Temperature={env_data.sht_temp}"),
    'bme': (lambda env_data : f"Temperature={env_data.bme_temp}"),
    'avg': (lambda env_data : f"Temperature={(float(env_data.bme_temp) + float(env_data.sht_temp)) / 2}")
} 

hum_str_gens = {
    'both': (lambda env_data : f"Relative_Humidity_BME={env_data.bme_humidity} Relative_Humidity_SHT={env_data.sht_humidity}"),
    'sht': (lambda env_data : f"Relative_Humidity={env_data.sht_humidity}"),
    'bme': (lambda env_data : f"Relative_Humidity={env_data.bme_humidity}"),
    'avg': (lambda env_data : f"Relative_Humidity={(float(env_data.bme_humidity) + float(env_data.sht_humidity)) / 2}")
} 

parser = argparse.ArgumentParser(prog="Sensor monitor")
parser.add_argument("-p", "--period", action="store", type=int, help="set time between consecutive measurements in seconds")
parser.add_argument("-g", "--get", action="store_true", help="read data from sensors and save it to log files")
parser.add_argument("-t", "--time", action="store_true", help="set sensors time to curent date")
parser.add_argument("-ts", "--temperature-source", action="store", type=str, default='avg', choices=['none', 'both', 'avg', 'bme', 'sht'], help="select temperatue source")
parser.add_argument("-hs", "--humidity-source", action="store", type=str, default='avg', choices=['none', 'both', 'avg', 'bme', 'sht'], help="select humidity source")
parser.add_argument("-ps", "--pressure-source", action="store", type=str, default='bme', choices=['none', 'bme'], help="select pressure source")
parser.add_argument("-fs", "--field-separator", action="store", type=str, default=' ', help="set field separator in log files")
parser.add_argument("-o", "--output-path", action="store", type=str, default='.', help="set log files output path")
parser.add_argument("--allow-invalid-names", action="store_true", help="allow invalid sensor names by prepending them with serial port name")
parser.add_argument("-v", "--verbose", action="store_true", help="enable verbose output")

def main():
    args = parser.parse_args()
    global verbose
    verbose = args.verbose

    if not args.period and not args.get and not args.time:
        parser.print_usage()
        return

    scan_devices(args.allow_invalid_names)

    if args.period:
        set_period(args.period)

    if args.time:
        set_time()

    if args.get:
        get_data(
            args.output_path,
            temp_str_gens.get(args.temperature_source),
            hum_str_gens.get(args.humidity_source),
            args.pressure_source != 'none',
            args.field_separator
        )

    for sensor in devices:
        sensor.serial.close()

if __name__ == "__main__":
    main()
