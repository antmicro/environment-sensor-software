*** Settings ***
Library     Process

*** Variables ***
${UART}                          sysbus.lpuart1
${BOARD_DESCRIPTION}             @${CURDIR}/sensor_board.repl
${MCU_DESCRIPTION}               @${CURDIR}/stm32g474.repl
${ELF}                           @${CURDIR}/../build/zephyr/zephyr.elf

*** Keywords ***
Create Machine And Wait For Boot
    Execute Command              mach create
    Execute Command              machine LoadPlatformDescription ${MCU_DESCRIPTION}
    Execute Command              machine LoadPlatformDescription ${BOARD_DESCRIPTION}
    Execute Command              sysbus LoadELF ${ELF}
    Create Terminal Tester       ${UART}
    Wait For Line On Uart        *** Booting Zephyr OS

Read Data
    [Arguments]                  ${acknowledge}
    ${data}=    run process      python3         ${CURDIR}/print_data_to_stdout.py
    Write Line To Uart           ${acknowledge}

    RETURN                       ${data.stdout}

Read Data Twice
    [Arguments]                  ${acknowledge}
    Create Machine And Wait For Boot
    Execute Command              emulation CreateUartPtyTerminal "pty_term" "/tmp/ttyUSB0"
    Execute Command              connector Connect sysbus.lpuart1 pty_term

    Write Line To Uart           set period 1
    Wait For Line On Uart        period set    pauseEmulation=true
    
    Execute Command              sysbus.i2c1.bme280 Temperature 20
    Execute Command              emulation RunFor "3"
    Execute Command              sysbus.i2c1.bme280 Temperature 25
    Execute Command              emulation RunFor "3"
    Execute Command              start

    ${data1}=                    Read Data       ${acknowledge}
    ${data2}=                    Read Data       ${acknowledge}

    RETURN                       ${data1}    ${data2}

*** Test Cases ***
Should Not Remove Data Without Ack
    ${data1}    ${data2}=        Read Data Twice    n
    Should Be True               """${data1}""" in """${data2}"""

Should Remove Data After Ack
    ${data1}    ${data2}=        Read Data Twice    y
    Should Be True               """${data1}""" not in """${data2}"""
