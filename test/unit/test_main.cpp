#include <unity.h>

void test_example(void) {
    TEST_ASSERT_EQUAL(2, 1 + 1);
}

#ifdef ARDUINO
void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_example);
    UNITY_END();
}

void loop() { }
#else
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_example);
    return UNITY_END();
}
#endif
