// src/utils/freeRTOSMutex.hpp
#ifndef FREERTOS_MUTEX_HPP
#define FREERTOS_MUTEX_HPP

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace GrowController {

/**
 * @brief Classe wrapper RAII para um Mutex FreeRTOS (SemaphoreHandle_t).
 * Garante que vSemaphoreDelete seja chamado quando o objeto sair de escopo.
 */
class FreeRTOSMutex {
    SemaphoreHandle_t handle = nullptr;

public:
    /**
     * @brief Constrói e cria o mutex subjacente.
     */
    FreeRTOSMutex() {
        handle = xSemaphoreCreateMutex();
        // Opcional: Adicionar log se handle == nullptr
    }

    /**
     * @brief Destrói o objeto e deleta o mutex subjacente, se válido.
     */
    ~FreeRTOSMutex() {
        if (handle != nullptr) {
            vSemaphoreDelete(handle);
            handle = nullptr; // Boa prática
        }
    }

    // --- Regra dos Cinco (Desabilitando Cópia, Habilitando Move) ---

    // 1. Desabilitar Construtor de Cópia
    FreeRTOSMutex(const FreeRTOSMutex&) = delete;

    // 2. Desabilitar Operador de Atribuição por Cópia
    FreeRTOSMutex& operator=(const FreeRTOSMutex&) = delete;

    // 3. Habilitar Construtor de Movimentação (Move Constructor)
    FreeRTOSMutex(FreeRTOSMutex&& other) noexcept : handle(other.handle) {
        // Após mover o handle, o 'other' não deve mais gerenciá-lo
        other.handle = nullptr;
    }

    // 4. Habilitar Operador de Atribuição por Movimentação (Move Assignment)
    FreeRTOSMutex& operator=(FreeRTOSMutex&& other) noexcept {
        // Proteção contra auto-atribuição (mover para si mesmo)
        if (this != &other) {
            // Libera o recurso atual (se existir)
            if (handle != nullptr) {
                vSemaphoreDelete(handle);
            }
            // Transfere a posse do recurso de 'other'
            handle = other.handle;
            // Garante que 'other' não gerencie mais o recurso
            other.handle = nullptr;
        }
        return *this;
    }

    // --- Métodos Utilitários ---

    /**
     * @brief Obtém o handle bruto do semáforo para uso com APIs FreeRTOS.
     * @return SemaphoreHandle_t O handle do mutex ou nullptr se a criação falhou.
     */
    SemaphoreHandle_t get() const {
        return handle;
    }

    /**
     * @brief Verifica se o mutex foi criado com sucesso.
     * @return true Se o handle não for nullptr, false caso contrário.
     */
    explicit operator bool() const {
        return handle != nullptr;
    }
};

} // namespace GrowController

#endif // FREERTOS_MUTEX_HPP