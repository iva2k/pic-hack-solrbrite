#!/bin/sh
# install-pickle.sh - run ON the Raspberry Pi (3B+/4B).
# Builds and installs Pickle (https://wiki.kewl.org/projects:pickle) and
# writes a GPIO config for low-voltage programming of the PIC12LF1572.
#
# Wiring (Pi physical pin -> PIC12LF1572 DIP pin), BCM GPIO numbers in ():
#   1  3V3          -> 1 VDD   (100 nF to GND at the PIC)
#   6  GND          -> 8 VSS
#   7  (GPIO4)  VPP -> 4 MCLR
#   22 (GPIO25) PGC -> 6 ICSPCLK  (optional 220R series)
#   18 (GPIO24) PGD -> 7 ICSPDAT  (optional 220R series)
set -e

VERSION="${PICKLE_VERSION:-5.01}"
URL="https://wiki.kewl.org/_media/downloads:pickle-${VERSION}.tgz"
WORK="${HOME}/src/pickle"

sudo apt-get update
sudo apt-get install -y build-essential

mkdir -p "${WORK}"
cd "${WORK}"
if [ ! -f "pickle-${VERSION}.tgz" ]; then
    wget -O "pickle-${VERSION}.tgz" "${URL}"
fi
tar zxf "pickle-${VERSION}.tgz"
cd pickle
make linux
sudo make linux-install

# Config: ~/.pickle (file). Raise SLEEP if programming is unreliable
# (long wires, in-circuit loads on PGC/PGD).
if [ -e "${HOME}/.pickle" ]; then
    echo "~/.pickle exists - not overwritten"
else
    cat > "${HOME}/.pickle" <<'EOF'
DEVICE=RPI
SLEEP=1
BITRULES=0x1000
VPP=4
PGM=-1
PGC=25
PGD=24
EOF
    echo "wrote ~/.pickle"
fi

echo "Test with the PIC wired:  p14 lvp id"
