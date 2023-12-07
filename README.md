# Antmicro Enviroment Sensor software

Copyright (c) 2023 [Antmicro](https://antmicro.com/)

This repository contains the Zephyr firmware and monitor app for the [Environment Sensor](https://github.com/antmicro/environment-sensor).

![environment-sensor](./img/envsensor.png "Environment Sensor board")

## Table of Contents
1. [Firmware](#firmware)
1. [Monitor](#monitor)

## Firmware

### Getting Started

Before getting started, make sure you have a proper Zephyr development
environment. Follow the official
[Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/getting_started/index.html).

#### West workspace initialization

Initialize a `west` workspace within the project directory:

```shell
pip install west
west init -l
west update
```

#### Building

Build the application, run the following command:

```shell
west build -b  nucleo_g474re
```

### Flashing the MCU

To program the app into the MCU's flash, connect a USB cable to the target and run:

#### Enviroment Sensor rev. 1.0.0

```shell
west flash --config ./ft4232h_jtag.cfg 
```
(will not work without the `./`)

#### Enviroment Sensor rev. 1.1.0

```shell
west flash --config ./ft232r_jtag.cfg
```

Alternatively, you may omit `west` by using [OpenOCD](https://github.com/openocd-org/openocd/tree/v0.11.0), clone it and compile with:

```shell
./bootstrap 
./configure --enable-ft232r
make
sudo make install
```


#### Enviroment Sensor rev. 1.0.0

```shell
openocd -f ./ft4232h_jtag.cfg -c "init; program build/zephyr/zephyr.elf verify reset exit"
```

For the board to autostart from flash, the `nBOOT0` option bit must be set. This needs to be done only once:
```shell
openocd -f ./ft4232h_jtag.cfg -c "init; stm32l4x option_write 0 0x20 0xfbeff8aa;" -c "stm32l4x option_read 0 0x20" -c "reset; exit"
```
#### Enviroment Sensor rev. 1.1.0
since rev. 1.1.0 uses diffrent FTDI chip, we can program it with:

```shell
openocd -f ./ft232r_jtag.cfg  -c "init; program build/zephyr/zephyr.elf verify reset exit"
```
For the board to autostart from flash (this needs to be done only once, when flashing for the first time):
```shell
openocd -f ./ft232r_jtag.cfg  -c "init; stm32l4x option_write 0 0x20 0xfbeff8aa;" -c "stm32l4x option_read 0 0x20" -c "reset; exit"
```


### Console

#### Enviroment Sensor rev. 1.0.0

```shell
picocom /dev/ttyUSB2 -b 115200
```

#### Enviroment Sensor rev. 1.1.0

```shell
picocom /dev/ttyUSB0 -b 115200
```

## Monitor

The sensor monitor is a Linux application that retrieves environmental data from sensors connected to the device it is being run on.

When the monitor is run, it looks for sensors in `/dev`. When a sensor device is found and its name is valid<sup>1</sup>, it adds it to the list of available devices.  
<sup>1</sup><sub>This behavior can be overridden by using `--allow-invalid-names`<sup>

### Installation

You need Python and `pip` installed. Run the following command:

`pip install git+https://github.com/antmicro/antmicro-environment-sensor-software.git#subdirectory=monitor`

### Usage

Use `antenvsens-monitor <args>` after installation. You can also use `antenvsens_monitor.py` directly from the monitor repository.

#### Arguments

The sensor monitor application can be run with following arguments:

* `--time` or `-t`  
Sets sensor time to current time
* `--period` or `-p <NUMBER>`  
Sets the time (in seconds) between consecutive measurements
* `--get` or `-g`  
Retrieves data and saves it to output files. The data is removed from the sensor after reading
* `--temperature-source` or `-ts <SOURCE>`  
Selects the source of temperature data. Available options are: `none`, `both`, `avg`, `bme`, `sht`. Defaults to `avg`. See the [Sources](sources) section
* `--humidity-source` or `-hs <SOURCE>`  
Selects the source of humidity data. Available options are: `none`, `both`, `avg`, `bme`, `sht`. Defaults to `avg`. See the [Sources](sources) section
* `--pressure-source` or `-ps <SOURCE>`  
Selects the source of pressure data. Available options are: `none`, `bme`. Defaults to `bme`. See the [Sources](sources) section
* `--field-separator` or `-fs <SEPARATOR>`  
Specifies the field separator. Defaults to `' '`
* `--output-path` or `-o <PATH>`  
Specifies the output path for the retrieved sensor data
* `--allow-invalid-names`  
Prepends device names with serial port names. This allows for unnamed devices or ones with duplicated names
* `--verbose` or `-v`  
Prints additional debug information during program execution

#### Sources

The Sensor board has two environmental sensors: Sensirion SHT45 and Bosch BME280. Both of them measure temperature and humidity. Additionally, BME280 measures pressure.  
The monitor script allows the following configurations of environmental data source:

* `none` - this particular measurement won't be included in the output
* `both` - both measured values will be included with suffixes specifying their sources. For example: `Temperature_BME=24.850000 Temperature_SHT=25.072098`
* `avg` - average of both sources will be included in log, without a suffix
* `bme` - only the measurement from BME280 will be included in the output, without a suffix
* `sht` - only the measurement from SHT45 will be included in the output, without a suffix

#### Output files

Each connected device has its own output file. Data points are stored in them line by line.  
Each line contains the following fields in this order: `Timestamp`, `Temperature`, `Humidity`, `Pressure`. Fields are separated by `--field-separator`

Example output line after running `antenvsens-monitor -g`:
```txt
2000-01-03T18:01:57 Temperature=26.1141755 Relative_Humidity=48.919128 Pressure=100.873621
```
