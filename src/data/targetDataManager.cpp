#include "targetDataManager.hpp"
#include <stdio.h> // Para sscanf

// Definir LOG_LEVEL_* se não estiver usando um logger centralizado ainda
#ifndef LOG_LEVEL_INFO
#define LOG_LEVEL_INFO 2 // Definir um valor (ex: 2)
#endif
#ifndef LOG_LEVEL_WARN
#define LOG_LEVEL_WARN 3
#endif
#ifndef LOG_LEVEL_ERROR
#define LOG_LEVEL_ERROR 4
#endif

// Função de log simples temporária (substituir pelo Logger depois)
static void temp_log(int level, const char *format, ...)
{
  // Log apenas INFO, WARN, ERROR por enquanto
  if (level >= LOG_LEVEL_INFO) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Serial.print(level == LOG_LEVEL_INFO ? "[INFO] " : (level == LOG_LEVEL_WARN ? "[WARN] " : "[ERROR] "));
    Serial.print("TargetMgr: ");
    Serial.println(buffer);
  }
}

namespace GrowController
{

  // --- Construtor ---
  TargetDataManager::TargetDataManager()
  {
    dataMutex = xSemaphoreCreateMutex();
    if (dataMutex == nullptr)
    {
      temp_log(LOG_LEVEL_ERROR, "Failed to create data mutex!");
      // Lidar com falha crítica? Abortar?
    }
    // Inicializar struct tm com zeros
    memset(&currentTargets.lightOnTime, 0, sizeof(struct tm)); 
    memset(&currentTargets.lightOffTime, 0, sizeof(struct tm));
    // Outros floats já inicializam como NAN no HPP
    temp_log(LOG_LEVEL_INFO, "TargetDataManager initialized."); // Use Logger::info depois
  }

  // --- Destrutor ---
  TargetDataManager::~TargetDataManager()
  {
    if (dataMutex != nullptr)
    {
      vSemaphoreDelete(dataMutex);
    }
  }

  // --- updateTargetsFromJson ---
  bool TargetDataManager::updateTargetsFromJson(const JsonDocument &doc)
  {
    bool updated = false;
    if (dataMutex == nullptr)
    {
      temp_log(LOG_LEVEL_ERROR, "Mutex not initialized in updateTargetsFromJson!");
      return false;
    }

    // Usar portMAX_DELAY para escrita, pois é uma operação crítica
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE)
    {
      // Atualiza os valores usando os helpers
      updated |= _updateFloatValue("airHumidity", doc, currentTargets.airHumidity);
      updated |= _updateFloatValue("vpd", doc, currentTargets.vpd);
      updated |= _updateFloatValue("soilHumidity", doc, currentTargets.soilHumidity);
      updated |= _updateFloatValue("temperature", doc, currentTargets.temperature);
      updated |= _updateTimeValue("lightOnTime", doc, currentTargets.lightOnTime);
      updated |= _updateTimeValue("lightOffTime", doc, currentTargets.lightOffTime);

      xSemaphoreGive(dataMutex);

      if (updated)
      {
        temp_log(LOG_LEVEL_INFO, "Targets updated via JSON."); // Log info opcional
      }
      else
      {
        temp_log(LOG_LEVEL_WARN, "Received JSON did not contain valid target keys/values.");
      }
    }
    else
    {
      temp_log(LOG_LEVEL_ERROR, "Failed to take mutex for updateTargetsFromJson!");
      // Isso não deveria acontecer com portMAX_DELAY a menos que haja um problema sério
    }
    return updated;
  }

  // --- getTargets ---
  TargetValues TargetDataManager::getTargets() const
  {
    TargetValues localCopy = {}; // Inicializa com defaults (NAN/0)
    // Inicializa struct tm com zeros
    memset(&localCopy.lightOnTime, 0, sizeof(struct tm));
    memset(&localCopy.lightOffTime, 0, sizeof(struct tm));

    if (dataMutex == nullptr)
    {
      temp_log(LOG_LEVEL_ERROR, "Mutex not initialized in getTargets!");
      return localCopy; // Retorna default
    }

    if (xSemaphoreTake(dataMutex, mutexTimeout) == pdTRUE)
    {
      localCopy = currentTargets; // Cópia da struct
      xSemaphoreGive(dataMutex);
    }
    else
    {
      temp_log(LOG_LEVEL_WARN, "Failed to take mutex for getTargets within timeout.");
      // Retorna a cópia inicializada com defaults/NAN
    }
    return localCopy;
  }

  // --- getTargetAirHumidity ---
  float TargetDataManager::getTargetAirHumidity() const
  {
    float value = NAN;
    if (dataMutex == nullptr)
    {
      temp_log(LOG_LEVEL_ERROR, "Mutex not initialized in getTargetAirHumidity!");
      return NAN;
    }

    if (xSemaphoreTake(dataMutex, mutexTimeout) == pdTRUE)
    {
      value = currentTargets.airHumidity;
      xSemaphoreGive(dataMutex);
    }
    else
    {
      temp_log(LOG_LEVEL_WARN, "Failed to take mutex for getTargetAirHumidity within timeout.");
    }
    return value;
  }

  // --- getLightOnTime ---
  struct tm TargetDataManager::getLightOnTime() const
  {
    struct tm timeCopy = {}; // Inicializa com zeros
    if (dataMutex == nullptr)
    {
      temp_log(LOG_LEVEL_ERROR, "Mutex not initialized in getLightOnTime!");
      return timeCopy;
    }

    if (xSemaphoreTake(dataMutex, mutexTimeout) == pdTRUE)
    {
      timeCopy = currentTargets.lightOnTime; // Cópia da struct
      xSemaphoreGive(dataMutex);
    }
    else
    {
      temp_log(LOG_LEVEL_WARN, "Failed to take mutex for getLightOnTime within timeout.");
    }
    return timeCopy;
  }

  // --- getLightOffTime ---
  struct tm TargetDataManager::getLightOffTime() const
  {
    struct tm timeCopy = {}; // Inicializa com zeros
    if (dataMutex == nullptr)
    {
      temp_log(LOG_LEVEL_ERROR, "Mutex not initialized in getLightOffTime!");
      return timeCopy;
    }
    if (xSemaphoreTake(dataMutex, mutexTimeout) == pdTRUE)
    {
      timeCopy = currentTargets.lightOffTime; // Cópia da struct
      xSemaphoreGive(dataMutex);
    }
    else
    {
      temp_log(LOG_LEVEL_WARN, "Failed to take mutex for getLightOffTime within timeout.");
    }
    return timeCopy;
  }

  // --- Helper _updateFloatValue ---
  bool TargetDataManager::_updateFloatValue(const char *key, const JsonDocument &doc, float &outValue)
  {
    if (!doc[key].isNull() && doc[key].is<float>())
    {
      outValue = doc[key].as<float>();
      temp_log(LOG_LEVEL_INFO, "Updated %s to: %.2f", key, outValue); // Log de debug opcional
      return true;
    }
    // Não logar erro se a chave não existir, apenas se existir mas for do tipo errado
    if (!doc[key].isNull() && !doc[key].is<float>())
    {
      temp_log(LOG_LEVEL_WARN, "JSON key '%s' exists but is not a float.", key);
    }
    return false;
  }

  // --- Helper _updateTimeValue ---
  bool TargetDataManager::_updateTimeValue(const char *key, const JsonDocument &doc, struct tm &outValue)
  {
    if (!doc[key].isNull() && doc[key].is<String>())
    {
      int h = -1, m = -1;
      // Usar as<const char*> para evitar cópia desnecessária da String
      int result = sscanf(doc[key].as<const char *>(), "%d:%d", &h, &m);
      if (result == 2 && h >= 0 && h <= 23 && m >= 0 && m <= 59)
      {
        outValue.tm_hour = h;
        outValue.tm_min = m;
        // Não mexer nos outros campos de tm (tm_sec, tm_year, etc.)
        temp_log(LOG_LEVEL_INFO, "Updated %s to: %02d:%02d", key, h, m); // Log de debug opcional
        return true;
      }
      else
      {
        temp_log(LOG_LEVEL_WARN, "Failed to parse time string for key '%s'. Expected HH:MM, got: %s", key, doc[key].as<const char *>());
      }
    }
    // Logar se existir mas não for string?
    if (!doc[key].isNull() && !doc[key].is<String>())
    {
      temp_log(LOG_LEVEL_WARN, "JSON key '%s' exists but is not a string.", key);
    }
    return false;
  }

} // namespace GrowController