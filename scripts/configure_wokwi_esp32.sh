#!/bin/bash
echo "[wokwi]" > ../wokwi.toml
echo "version = 1" >> ../wokwi.toml
echo "elf = \".pio/build/esp32/firmware.elf\"" >> ../wokwi.toml
echo "firmware = \".pio/build/esp32/firmware.bin\"" >> ../wokwi.toml
echo "Configurado para ESP32"