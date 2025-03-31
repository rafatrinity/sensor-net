#include <unity.h>
#include "sensors/sensorManager.hpp" // Inclui a declaração de calculateVpd

void test_calculateVpd(void) {
    // Caso 1: Temp = 25°C, Umidade = 50%, Esperado ≈ 1.5845 kPa
    float temp = 25.0;
    float humidity = 50.0;
    float expected_vpd = 1.5845;
    float actual_vpd = calculateVpd(temp, humidity);
    TEST_ASSERT_FLOAT_WITHIN(0.01, expected_vpd, actual_vpd);

    // Caso 2: Temp = 30°C, Umidade = 60%, Esperado ≈ 1.6972 kPa
    temp = 30.0;
    humidity = 60.0;
    expected_vpd = 1.6972;
    actual_vpd = calculateVpd(temp, humidity);
    TEST_ASSERT_FLOAT_WITHIN(0.01, expected_vpd, actual_vpd);

    // Caso 3: Temp = 25°C, Umidade = 100%, Esperado = 0.0 kPa
    temp = 25.0;
    humidity = 100.0;
    expected_vpd = 0.0;
    actual_vpd = calculateVpd(temp, humidity);
    TEST_ASSERT_FLOAT_WITHIN(0.01, expected_vpd, actual_vpd);
}

#ifdef ARDUINO
void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_calculateVpd);
    UNITY_END();
}

void loop() { }
#else
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_calculateVpd);
    return UNITY_END();
}
#endif