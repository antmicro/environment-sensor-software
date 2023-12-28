#!/bin/bash

if [ -n "$GITHUB_ACTION" ]; then
    s=sudo
else
    s=
fi

cd firmware
$s apt -qy update > /dev/null 2> /dev/null

if [ "$1" == "check-formatting" ]; then
    $s apt -qy install git clang-format-15 > /dev/null 2> /dev/null
    find src/ -iname '*.h' -o -iname '*.cpp' | xargs clang-format-15 -i --style=file:.clang-format
    echo "$(git status --porcelain)"
    exit $([ -z "$(git status --porcelain)" ]);
fi

if [ "$1" == "build-firmware" ]; then
    west init -l .
    west update
    west build -b nucleo_g474re
fi

if [ "$1" == "test-firmware" ]; then
    $s apt -qy install python3-dev git gcc pipx python3-venv > /dev/null 2> /dev/null
    pipx install git+https://github.com/antmicro/renode-run
    renode-run download
    python3 -m venv renode-test
    source renode-test/bin/activate
    pip3 install -r ~/.config/renode/renode-run.download/renode_*_portable/tests/requirements.txt
    pip3 install pyserial
    renode-run test --venv renode-test -- tests/simple_tests.robot
    renode-run test --venv renode-test -- tests/complex_tests.robot
fi
