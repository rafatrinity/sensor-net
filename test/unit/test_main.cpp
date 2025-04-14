#include <unity.h>
#include "sensors/sensorManager.hpp" // Inclui a declaração de calculateVpd
#include <cmath> // Incluir para isnan se usar no teste (ou confiar que a função interna faz)

using GrowController::SensorManager; // Opcional: para evitar digitar GrowController::SensorManager:: repetidamente

void test_calculateVpd(void) {
    // Caso 1: Temp = 25°C, Umidade = 50%, Esperado ≈ 1.584 kPa (recalculado/confirmado)
    float temp1 = 25.0;
    float humidity1 = 50.0;
    // SVP a 25C ≈ 3.1687 kPa
    // AVP = 3.1687 * 0.50 = 1.58435 kPa
    // VPD = 3.1687 - 1.58435 = 1.58435 kPa
    float expected_vpd1 = 1.5844; // Ajustado ligeiramente
   float actual_vpd1 = SensorManager::calculateVpd(temp1, humidity1); // Chamada estática
   TEST_ASSERT_FLOAT_WITHIN(0.01, expected_vpd1, actual_vpd1);

    // Caso 2: Temp = 30°C, Umidade = 60%, Esperado ≈ 1.697 kPa (recalculado/confirmado)
    float temp2 = 30.0;
    float humidity2 = 60.0;
    // SVP a 30C ≈ 4.243 kPa
    // AVP = 4.243 * 0.60 = 2.5458 kPa
    // VPD = 4.243 - 2.5458 = 1.6972 kPa
    float expected_vpd2 = 1.6972;
   float actual_vpd2 = SensorManager::calculateVpd(temp2, humidity2); // Chamada estática
   TEST_ASSERT_FLOAT_WITHIN(0.01, expected_vpd2, actual_vpd2);

    // Caso 3: Temp = 25°C, Umidade = 100%, Esperado = 0.0 kPa
    float temp3 = 25.0;
    float humidity3 = 100.0;
    float expected_vpd3 = 0.0;
   float actual_vpd3 = SensorManager::calculateVpd(temp3, humidity3); // Chamada estática
   TEST_ASSERT_FLOAT_WITHIN(0.01, expected_vpd3, actual_vpd3);

   // Caso 4: Teste com NaN (espera NaN)
   float temp4 = NAN;
   float humidity4 = 50.0;
   float actual_vpd4 = SensorManager::calculateVpd(temp4, humidity4);
   TEST_ASSERT_TRUE(isnan(actual_vpd4)); // Verifica se o resultado é NaN

   // Caso 5: Teste com umidade inválida (espera NaN)
   float temp5 = 20.0;
   float humidity5 = 110.0; // Inválido
   float actual_vpd5 = SensorManager::calculateVpd(temp5, humidity5);
   TEST_ASSERT_TRUE(isnan(actual_vpd5));
}

#ifdef ARDUINO
#include <Arduino.h> // Necessário para setup/loop no Arduino
void setup() {
   delay(2000); // Delay para dar tempo ao monitor serial de conectar
    UNITY_BEGIN();
    RUN_TEST(test_calculateVpd);
    UNITY_END();
}

void loop() {
    // Nada a fazer aqui para este teste
    delay(500);
}
#else // Para execução nativa (se configurado)
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_calculateVpd);
    return UNITY_END();
}
#endif