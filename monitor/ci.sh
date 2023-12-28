#!/bin/bash

if [ -n "$GITHUB_ACTION" ]; then
    s=sudo
else
    s=
fi

cd monitor
$s apt -qy update > /dev/null 2> /dev/null
$s apt -qy install python3-dev git gcc pipx python3-venv > /dev/null 2> /dev/null
pipx install git+https://github.com/antmicro/renode-run
renode-run download
python3 -m venv renode-test
source renode-test/bin/activate
pip3 install -r ~/.config/renode/renode-run.download/renode_*_portable/tests/requirements.txt
pip3 install -r requirements.txt
$s ln -s /tmp/ttyUSB0 /dev/ttyUSB0
$s ln -s /tmp/ttyUSB1 /dev/ttyUSB1
$s ln -s /tmp/ttyUSB2 /dev/ttyUSB2
cd tests
renode-run test --venv ../renode-test -- multinode.robot