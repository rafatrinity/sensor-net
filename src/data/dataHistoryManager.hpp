// src/data/dataHistoryManager.hpp
#ifndef DATA_HISTORY_MANAGER_HPP
#define DATA_HISTORY_MANAGER_HPP

#include "historicDataPoint.hpp"
#include <LittleFS.h>
#include <Preferences.h>
#include <vector>
#include <Arduino.h>            // Para String, Serial (se usado para logs)
#include "utils/logger.hpp"
#include "utils/freeRTOSMutex.hpp" // Para FreeRTOSMutex

namespace GrowController {

class DataHistoryManager {
public:
    DataHistoryManager();
    ~DataHistoryManager();

    DataHistoryManager(const DataHistoryManager&) = delete;
    DataHistoryManager& operator=(const DataHistoryManager&) = delete;

    bool initialize(const char* nvs_namespace = "history_mgr");
    bool addDataPoint(const HistoricDataPoint& dataPoint);
    std::vector<HistoricDataPoint> getAllDataPointsSorted();
    size_t getRecordCount() const;
    uint8_t getNextWriteIndex() const;

private:
    static const char* LOG_FILE_NAME;
    static const int MAX_RECORDS = 48;
    static const char* NVS_KEY_NEXT_INDEX;
    static const char* NVS_KEY_RECORD_COUNT;

    Preferences preferences;
    uint8_t nextWriteIndex;
    uint8_t recordCount;
    bool initializedState;
    const char* nvsNamespace;

    mutable FreeRTOSMutex dataMutex; // Mutex para proteger acesso concorrente
    static const TickType_t MUTEX_TIMEOUT_MS = pdMS_TO_TICKS(200);
};

} // namespace GrowController

#endif // DATA_HISTORY_MANAGER_HPP