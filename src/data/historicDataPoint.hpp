// src/data/historicDataPoint.hpp
#ifndef HISTORIC_DATA_POINT_HPP
#define HISTORIC_DATA_POINT_HPP

#include <stdint.h> // Para uint32_t
#include <math.h>   // Para NAN, se usado diretamente aqui (geralmente não)

namespace GrowController {

struct HistoricDataPoint {
    uint32_t timestamp;       // Unix timestamp UTC
    float avgTemperature;
    float avgAirHumidity;
    float avgSoilHumidity;
    float avgVpd;
    // bool timeIsValid; // Opcional: para indicar se o timestamp é de NTP ou relativo
};

} // namespace GrowController

#endif // HISTORIC_DATA_POINT_HPP