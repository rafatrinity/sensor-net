#!/bin/bash

ENV=$1

if [ "$ENV" == "c3" ]; then
    echo "Compilando e configurando para ESP32-C3..."
    ./scripts/compile_c3.sh 
    ./scripts/configure_wokwi_c3.sh
else
    echo "Compilando e configurando para ESP32..."
    ./scripts/compile_esp32.sh
    ./scripts/configure_wokwi_esp32.sh
fi