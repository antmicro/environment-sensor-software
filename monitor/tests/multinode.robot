*** Settings ***
Library     Process
Library     OperatingSystem
Library     DateTime
Library     disconnect_envsens_during_reading.py

*** Variables ***
${UART}                      sysbus.lpuart1
${BOARD_DESCRIPTION}         @${CURDIR}/sensor_board.repl
${MCU_DESCRIPTION}           @${CURDIR}/stm32g474.repl
${ELF}                       @${CURDIR}/../../firmware/build/zephyr/zephyr.elf
${TMP_PATH}                  ${CURDIR}/tmp

*** Keywords ***
Create Envsens
    [Arguments]              ${name}    ${dev_path}
    Execute Command          emulation CreateUartPtyTerminal "${name}term" "${dev_path}"
    Execute Command          mach create "${name}"
    Execute Command          machine LoadPlatformDescription ${MCU_DESCRIPTION}
    Execute Command          machine LoadPlatformDescription ${BOARD_DESCRIPTION}
    Execute Command          sysbus LoadELF ${ELF}
    ${tester_id}=            Create Terminal Tester   ${UART}    machine=${name}
    Execute Command          connector Connect sysbus.lpuart1 ${name}term

    Start Emulation
    Wait For Line On Uart    *** Booting Zephyr OS    testerId=${tester_id}
    Write Line To Uart       set name ${name}    testerId=${tester_id}
    Wait for line On Uart    name set    testerId=${tester_id}

    RETURN                   ${tester_id}

Set Sensors Values
    [Arguments]               ${name}    ${sht_t}    ${sht_h}    ${bme_t}    ${bme_h}    ${bme_p}
    Execute Command           mach set "${name}"
    Execute Command           sysbus.i2c1.sht45 Temperature ${sht_t}
    Execute Command           sysbus.i2c1.sht45 Humidity ${sht_h}
    Execute Command           sysbus.i2c1.bme280 Temperature ${bme_t}
    Execute Command           sysbus.i2c1.bme280 Humidity ${bme_h}
    Execute Command           sysbus.i2c1.bme280 Pressure ${bme_p}

Get Board Time
    [Arguments]               ${tester_id}
    Write Line To Uart        get status     testerId=${tester_id}
    ${out}=                   Wait For Line On Uart    ^time:    testerId=${tester_id}    treatAsRegex=true
    ${time_str}=              Get Substring    ${out.line}    6
    ${time}=                  Convert Date    ${time_str}    date_format=%Y-%m-%dT%H:%M:%S
    RETURN                    ${time}

*** Settings ***
Test Teardown    Remove Directory    ${TMP_PATH}    recursive=true

*** Test Cases ***
Should Save Data To Files
    Create Envsens                  envsens0        /tmp/ttyUSB0
    Create Envsens                  envsens1        /tmp/ttyUSB1
    Create Envsens                  envsens2        /tmp/ttyUSB2

    Set Sensors Values              envsens0    20    40    20    40    1000
    Set Sensors Values              envsens1    25    40    20    40    1000
    Set Sensors Values              envsens2    30    40    20    40    1000

    Execute Command                 pause
    Execute Command                 emulation RunFor "3"
    Execute Command                 start

    run process    python3    ${CURDIR}/../antenvsens_monitor.py    -g    -v    -o    ${TMP_PATH}

    File Should Exist               ${CURDIR}/tmp/envsens0
    File Should Exist               ${CURDIR}/tmp/envsens1
    File Should Exist               ${CURDIR}/tmp/envsens2

    ${file0}=                       Get File    ${CURDIR}/tmp/envsens0
    Should Contain                  ${file0}    Temperature=20.000572 Relative_Humidity=40.023551499999996 Pressure=999.084414
    ${file1}=                       Get File    ${CURDIR}/tmp/envsens1
    Should Contain                  ${file1}    Temperature=22.5 Relative_Humidity=40.023551499999996 Pressure=999.084414
    ${file2}=                       Get File    ${CURDIR}/tmp/envsens2
    Should Contain                  ${file2}    Temperature=24.9994275 Relative_Humidity=40.023551499999996 Pressure=999.084414

Should Handle Improperly Terminated Read
    ${id}=                          Create Envsens    envsens0    /tmp/ttyUSB0
    Set Sensors Values              envsens0    20    40    20    40    1000
    Execute Command                 pause
    Execute Command                 emulation RunFor "3"
    Set Sensors Values              envsens0    30    40    20    40    1000
    Execute Command                 emulation RunFor "3"
    Execute Command                 start
    Write Line To Uart              get data    testerId=${id}
    Wait For Prompt On Uart         remove printed data from the device? (y/N):    testerId=${id}


    run process    python3    ${CURDIR}/../antenvsens_monitor.py    -g    -v    -o    ${TMP_PATH}

    File Should Exist               ${CURDIR}/tmp/envsens0
    ${file0}=                       Get File    ${CURDIR}/tmp/envsens0
    Should Contain                  ${file0}    Temperature=20.000572 Relative_Humidity=40.023551499999996 Pressure=999.084414
    Should Contain                  ${file0}    Temperature=24.9994275 Relative_Humidity=40.023551499999996 Pressure=999.084414

Should Not Interact With Already Used TTY
    Create Envsens                  envsens0        /tmp/ttyUSB0

    Execute Command                 pause
    Execute Command                 emulation RunFor "3"
    Execute Command                 start

    ${res}=                      run process    python3    ${CURDIR}/launch_monitor_with_tty_in_use.py

    Directory Should Not Exist      ${CURDIR}/tmp
    Should Be True                  """serial exception on /dev/ttyUSB0""" in """${res.stdout}"""

Should Set Period
    ${tester0}=                     Create Envsens                  envsens0        /tmp/ttyUSB0
    ${tester1}=                     Create Envsens                  envsens1        /tmp/ttyUSB1
    ${tester2}=                     Create Envsens                  envsens2        /tmp/ttyUSB2

    ${period}=                      Set Variable    17

    run process                     python3    ${CURDIR}/../antenvsens_monitor.py    -p    ${period}    -v    -o    ${TMP_PATH}

    Write Line To Uart              get period     testerId=${tester0}
    Wait For Line On Uart           ${period}      testerId=${tester0}
    Write Line To Uart              get period     testerId=${tester1}
    Wait For Line On Uart           ${period}      testerId=${tester1}
    Write Line To Uart              get period     testerId=${tester2}
    Wait For Line On Uart           ${period}      testerId=${tester2}

Should Set Date
    ${tester0}=                     Create Envsens                  envsens0        /tmp/ttyUSB0
    ${tester1}=                     Create Envsens                  envsens1        /tmp/ttyUSB1
    ${tester2}=                     Create Envsens                  envsens2        /tmp/ttyUSB2

    ${curr_time}=                   Get Current Date
    ${future_time}=                 Add Time To Date    ${curr_time}    30d    result_format=%Y-%m-%dT%H:%M:%S    exclude_millis=yes

    Write Line To Uart              set time ${future_time}    testerId=${tester0}
    Write Line To Uart              set time ${future_time}    testerId=${tester1}
    Write Line To Uart              set time ${future_time}    testerId=${tester2}

    run process                     python3    ${CURDIR}/../antenvsens_monitor.py    -t    -v    -o    ${TMP_PATH}    #sets current time

    ${time0}=                       Get Board Time    ${tester0}
    ${time1}=                       Get Board Time    ${tester0}
    ${time2}=                       Get Board Time    ${tester0}
    Should Be True                  $time0 < $future_time 
    Should Be True                  $time1 < $future_time
    Should Be True                  $time2 < $future_time

Should Handle Disconnection Of Envsens During Reading
    Create Envsens                  envsens0        /tmp/ttyUSB0

    Execute Command                 pause
    Execute Command                 emulation RunFor "3"
    Execute Command                 start
    
    ${res}=     run process         python3    ${CURDIR}/disconnect_envsens_during_reading.py

    Should Be Equal As Integers     ${res.rc}    0
    Directory Should Be Empty       ${CURDIR}/tmp
