*** Variables ***
${UART}                      sysbus.lpuart1
${BOARD_DESCRIPTION}         @${CURDIR}/sensor_board.repl
${MCU_DESCRIPTION}           @${CURDIR}/stm32g474.repl
${ELF}                       @${CURDIR}/../build/zephyr/zephyr.elf

*** Keywords ***
Create Machine
    Execute Command          mach create
    Execute Command          machine LoadPlatformDescription ${MCU_DESCRIPTION}
    Execute Command          machine LoadPlatformDescription ${BOARD_DESCRIPTION}
    Execute Command          sysbus LoadELF ${ELF}
    Create Terminal Tester   ${UART}

Create Machine And Wait For Boot
    Create Machine
    Wait For Line On Uart     *** Booting Zephyr OS

*** Test Cases ***
Should Set Name
    Create Machine And Wait For Boot
    
    Write Line To Uart        set name dev
    Write Line To Uart        get name
    Wait For Line On Uart     dev

Should Confirm Set Name
    Create Machine And Wait For Boot

    Write Line To Uart        set name dev
    Wait For Line On Uart     name set

Should Set Period
    Create Machine And Wait For Boot

    Write Line To Uart        set period 60
    Write Line To Uart        get period
    Wait For Line On Uart     60

Should Confirm Set Period
    Create Machine And Wait For Boot

    Write Line To Uart        set period 60
    Wait For Line On Uart     period set

Should Set Time
    Create Machine And Wait For Boot

    Write Line To Uart        set time 2023-09-26T12:00:00
    Write Line To Uart        get time
    Wait For Line On Uart     2023-09-26T12:00:00

Should Confirm Set Time
    Create Machine And Wait For Boot

    Write Line To Uart        set time 2023-09-26T12:00:00
    Wait For Line On Uart     time set

Should Print Help
    Create Machine And Wait For Boot

    Write Line To Uart        help
    Wait For Line On Uart     available commands:

Should Inform About Invalid Command
    Create Machine And Wait For Boot

    Write Line To Uart        sibdfubvubu
    Wait For Line On Uart     invalid command

Should Print Measurement Data
    Create Machine

    Start Emulation

    Execute Command           sysbus.i2c1.sht45 Temperature 20
    Execute Command           sysbus.i2c1.bme280 Temperature 20
    Execute Command           sysbus.i2c1.sht45 Humidity 40
    Execute Command           sysbus.i2c1.bme280 Humidity 40
    Execute Command           sysbus.i2c1.bme280 Pressure 1000

    Wait For Line On Uart     *** Booting Zephyr OS
    Write Line To Uart        get data
    Wait For Line On Uart     ,20.000000,999.084414,40.046875,20.001144,40.000228

Should Ask For Confirmation After Printing Measurement Data
    Create Machine And Wait For Boot

    Write Line To Uart        get data
    Wait For Prompt On Uart   remove printed data from the device? (y/N): 