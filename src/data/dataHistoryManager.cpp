// src/data/dataHistoryManager.cpp
#include "dataHistoryManager.hpp"
#include <algorithm> // Para std::sort (se a ordenação explícita fosse necessária)

namespace GrowController {

// Definição das constantes estáticas
const char* DataHistoryManager::LOG_FILE_NAME = "/sensor_log.dat";
const char* DataHistoryManager::NVS_KEY_NEXT_INDEX = "hist_next_idx";
const char* DataHistoryManager::NVS_KEY_RECORD_COUNT = "hist_rec_cnt";
// MAX_RECORDS já é definido no .hpp

DataHistoryManager::DataHistoryManager() :
    nextWriteIndex(0),
    recordCount(0),
    initializedState(false),
    nvsNamespace(nullptr)
    // dataMutex é inicializado automaticamente pelo seu construtor
{
    // Logger::info("DataHistoryManager: Constructor called.");
}

DataHistoryManager::~DataHistoryManager() {
    // if (preferences.isOpened()) {
    //     preferences.end();
    // }
    // Logger::info("DataHistoryManager: Destructor called.");
}

bool DataHistoryManager::initialize(const char* nvs_name) {
    if (initializedState) {
        Logger::warn("DataHistoryManager: Already initialized.");
        return true;
    }
     if (!dataMutex) { // Verifica se o mutex foi criado com sucesso
        Logger::error("DataHistoryManager: Failed to create dataMutex!");
        return false;
    }

    this->nvsNamespace = nvs_name;
    Logger::info("DataHistoryManager: Initializing with NVS namespace '%s'...", this->nvsNamespace);

    // LittleFS.begin() deve ser chamado externamente
    if (!LittleFS.exists(LOG_FILE_NAME)) {
        Logger::info("DataHistoryManager: Log file '%s' not found, attempting to create it.", LOG_FILE_NAME);
        File file = LittleFS.open(LOG_FILE_NAME, "w"); // "w" para criar
        if (!file) {
            Logger::error("DataHistoryManager: Failed to create log file '%s'.", LOG_FILE_NAME);
            return false;
        }
        // Opcional: Pré-alocar o arquivo para o tamanho máximo para garantir seeks válidos
        // HistoricDataPoint emptyPoint = {0};
        // for (int i = 0; i < MAX_RECORDS; ++i) {
        //     if(file.write((const uint8_t*)&emptyPoint, sizeof(HistoricDataPoint)) != sizeof(HistoricDataPoint)){
        //         Logger::error("DataHistoryManager: Failed to write empty point during pre-allocation.");
        //         file.close();
        //         return false;
        //     }
        // }
        // Logger::info("DataHistoryManager: Log file pre-allocated to %d records.", MAX_RECORDS);
        file.close();
    } else {
        Logger::info("DataHistoryManager: Log file '%s' found.", LOG_FILE_NAME);
        File file = LittleFS.open(LOG_FILE_NAME, "r");
        if(file) {
            Logger::info("DataHistoryManager: Log file size: %u bytes.", file.size());
            file.close();
        }
    }

    // Proteger leitura da NVS e inicialização de membros
    if (xSemaphoreTake(dataMutex.get(), MUTEX_TIMEOUT_MS) == pdTRUE) {
        if (!preferences.begin(this->nvsNamespace, false)) { // false = read/write
            Logger::error("DataHistoryManager: Failed to open NVS namespace '%s'.", this->nvsNamespace);
            xSemaphoreGive(dataMutex.get());
            return false;
        }

        nextWriteIndex = preferences.getUChar(NVS_KEY_NEXT_INDEX, 0);
        recordCount = preferences.getUChar(NVS_KEY_RECORD_COUNT, 0);

        if (nextWriteIndex >= MAX_RECORDS) {
            Logger::warn("DataHistoryManager: Invalid nextWriteIndex (0x%X) from NVS. Resetting to 0.", nextWriteIndex);
            nextWriteIndex = 0;
        }
        if (recordCount > MAX_RECORDS) {
            Logger::warn("DataHistoryManager: Invalid recordCount (%u) from NVS. Resetting to 0.", recordCount);
            recordCount = 0;
        }
        preferences.end(); // Fechar NVS após leitura inicial

        initializedState = true;
        Logger::info("DataHistoryManager: Initialized. NextWriteIndex: %u, RecordCount: %u", nextWriteIndex, recordCount);
        xSemaphoreGive(dataMutex.get());
        return true;
    } else {
        Logger::error("DataHistoryManager: Timed out acquiring mutex for initialization.");
        return false;
    }
}

bool DataHistoryManager::addDataPoint(const HistoricDataPoint& dataPoint) {
    if (!initializedState) {
        Logger::error("DataHistoryManager: Not initialized. Cannot add data point.");
        return false;
    }

    if (xSemaphoreTake(dataMutex.get(), MUTEX_TIMEOUT_MS) != pdTRUE) {
        Logger::error("DataHistoryManager: Timed out acquiring mutex for addDataPoint.");
        return false;
    }

    // Logger::debug("DataHistoryManager: Adding data point at index %u", nextWriteIndex);

    File file = LittleFS.open(LOG_FILE_NAME, "r+");
    if (!file) { // Se "r+" falhar em abrir (ex: arquivo não existe e "r+" não cria em algumas implementações)
        file = LittleFS.open(LOG_FILE_NAME, "w+"); // Tenta criar e abrir para r/w
        if (!file) {
            Logger::error("DataHistoryManager: Failed to open or create log file '%s' for writing.", LOG_FILE_NAME);
            xSemaphoreGive(dataMutex.get());
            return false;
        }
        // Logger::info("DataHistoryManager: Log file '%s' created on demand by addDataPoint.", LOG_FILE_NAME);
    }

    size_t seekPosition = (size_t)nextWriteIndex * sizeof(HistoricDataPoint);
    if (!file.seek(seekPosition)) {
        Logger::error("DataHistoryManager: Failed to seek to position %u (offset %u) in log file.", nextWriteIndex, seekPosition);
        file.close();
        xSemaphoreGive(dataMutex.get());
        return false;
    }

    size_t bytesWritten = file.write((const uint8_t*)&dataPoint, sizeof(HistoricDataPoint));
    if (bytesWritten != sizeof(HistoricDataPoint)) {
        Logger::error("DataHistoryManager: Failed to write data point. Expected %u bytes, wrote %u.", sizeof(HistoricDataPoint), bytesWritten);
        file.close();
        xSemaphoreGive(dataMutex.get());
        return false;
    }
    file.flush(); // Garante que os dados sejam escritos na flash
    file.close();

    // Atualizar os índices em memória
    uint8_t oldNextWriteIndexForNVS = nextWriteIndex; // Salva para possível rollback da NVS
    uint8_t oldRecordCountForNVS = recordCount;     // Salva para possível rollback da NVS

    nextWriteIndex = (nextWriteIndex + 1) % MAX_RECORDS;
    if (recordCount < MAX_RECORDS) {
        recordCount++;
    }

    // Salvar os novos índices na NVS
    bool nvsOk = false;
    if (preferences.begin(this->nvsNamespace, false)) {
        if (preferences.putUChar(NVS_KEY_NEXT_INDEX, nextWriteIndex) &&
            preferences.putUChar(NVS_KEY_RECORD_COUNT, recordCount)) {
            nvsOk = true;
        } else {
            Logger::error("DataHistoryManager: Failed to save one or both indices to NVS.");
        }
        preferences.end();
    } else {
        Logger::error("DataHistoryManager: Failed to open NVS namespace '%s' for saving indices.", this->nvsNamespace);
    }
    
    if (!nvsOk) {
        // Se a escrita na NVS falhou, os índices em memória estão à frente do que está persistido.
        // Reverter os índices em memória para o estado anterior (consistente com a NVS) é uma opção,
        // mas o dado JÁ FOI escrito no arquivo. A próxima escrita (se os índices forem revertidos)
        // sobrescreveria o dado recém-escrito.
        // Isso é um estado problemático. A melhor ação é logar e aceitar a possível inconsistência
        // até que a NVS possa ser escrita novamente.
        Logger::warn("DataHistoryManager: NVS save failed. In-memory indices (next:%u, count:%u) may be ahead of NVS (next:%u, count:%u). Data was written to file.",
                     nextWriteIndex, recordCount, oldNextWriteIndexForNVS, oldRecordCountForNVS);
        // Não reverter nextWriteIndex e recordCount em memória, pois o arquivo FOI modificado.
        // A próxima inicialização lerá os valores antigos da NVS e poderá causar uma perda de dados percebida
        // ou sobrescrita do último dado se o ESP reiniciar antes da NVS ser atualizada.
        xSemaphoreGive(dataMutex.get());
        return false; // Indica falha na persistência completa.
    }

    // Logger::info("DataHistoryManager: Data point added. New nextWriteIndex: %u, recordCount: %u. Indices saved to NVS.", nextWriteIndex, recordCount);
    xSemaphoreGive(dataMutex.get());
    return true;
}

std::vector<HistoricDataPoint> DataHistoryManager::getAllDataPointsSorted() {
    std::vector<HistoricDataPoint> points;
    if (!initializedState) {
        Logger::error("DataHistoryManager: Not initialized. Cannot get data points.");
        return points;
    }

    uint8_t localNextWriteIndex;
    uint8_t localRecordCount;

    // Pega uma cópia consistente dos índices atuais sob proteção do mutex
    if (xSemaphoreTake(dataMutex.get(), MUTEX_TIMEOUT_MS) == pdTRUE) {
        localNextWriteIndex = this->nextWriteIndex;
        localRecordCount = this->recordCount;
        xSemaphoreGive(dataMutex.get());
    } else {
        Logger::error("DataHistoryManager: Timed out acquiring mutex for getAllDataPointsSorted (index read).");
        return points;
    }
    
    if (localRecordCount == 0) {
        // Logger::info("DataHistoryManager: No records to retrieve.");
        return points;
    }

    File file = LittleFS.open(LOG_FILE_NAME, "r");
    if (!file) {
        Logger::error("DataHistoryManager: Failed to open log file '%s' for reading.", LOG_FILE_NAME);
        return points;
    }

    points.reserve(localRecordCount);
    HistoricDataPoint currentPoint;

    if (localRecordCount < MAX_RECORDS) {
        // O buffer ainda não está cheio ou não girou completamente.
        // Os dados estão em ordem de 0 até localRecordCount-1.
        // Logger::debug("DataHistoryManager: Reading %u records (buffer not full/wrapped).", localRecordCount);
        for (uint8_t i = 0; i < localRecordCount; ++i) {
            if (file.seek((size_t)i * sizeof(HistoricDataPoint))) {
              if (file.read((uint8_t *)&currentPoint, sizeof(HistoricDataPoint)) == sizeof(HistoricDataPoint))
              {
                points.push_back(currentPoint);
              }
              else
              {
                Logger::error("DataHistoryManager: Failed to read record at index %u.", i);
                break;
              }
            } else {
                 Logger::error("DataHistoryManager: Failed to seek to record at index %u.", i);
                 break;
            }
        }
    } else { // localRecordCount == MAX_RECORDS, o buffer está cheio e pode ter girado.
        // localNextWriteIndex aponta para o registro MAIS ANTIGO.
        // A ordem cronológica começa em localNextWriteIndex, vai até MAX_RECORDS-1,
        // e então continua de 0 até localNextWriteIndex-1.
        // Logger::debug("DataHistoryManager: Reading %u records (buffer full/wrapped). Start index for oldest: %u", localRecordCount, localNextWriteIndex);

        for (uint8_t i = 0; i < MAX_RECORDS; ++i) {
            // Calcula o índice real no arquivo para ler em ordem cronológica
            uint8_t actualFileIndex = (localNextWriteIndex + i) % MAX_RECORDS;
            if (file.seek((size_t)actualFileIndex * sizeof(HistoricDataPoint))) {
              if (file.read((uint8_t *)&currentPoint, sizeof(HistoricDataPoint)) == sizeof(HistoricDataPoint))
              {
                points.push_back(currentPoint);
              }
              else
              {
                Logger::error("DataHistoryManager: Failed to read record at actual file index %u (logical order %u).", actualFileIndex, i);
                break;
              }
            } else {
                Logger::error("DataHistoryManager: Failed to seek to record at actual file index %u.", actualFileIndex);
                break;
            }
        }
    }

    file.close();
    // Logger::info("DataHistoryManager: Retrieved %u data points.", points.size());
    return points;
}

size_t DataHistoryManager::getRecordCount() const {
    size_t count = 0;
    if (xSemaphoreTake(dataMutex.get(), MUTEX_TIMEOUT_MS) == pdTRUE) {
        count = recordCount;
        xSemaphoreGive(dataMutex.get());
    } else {
        // Não logar erro aqui para não poluir, mas a leitura pode ser 0 se o mutex falhar.
        // Logger::warn("DataHistoryManager: Timed out acquiring mutex for getRecordCount.");
    }
    return count;
}

uint8_t DataHistoryManager::getNextWriteIndex() const {
    uint8_t idx = 0; // Valor padrão seguro
     if (xSemaphoreTake(dataMutex.get(), MUTEX_TIMEOUT_MS) == pdTRUE) {
        idx = nextWriteIndex;
        xSemaphoreGive(dataMutex.get());
    } else {
        // Logger::warn("DataHistoryManager: Timed out acquiring mutex for getNextWriteIndex.");
    }
    return idx;
}

} // namespace GrowController