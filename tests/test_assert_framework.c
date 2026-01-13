/**
 * @file test_assert_framework.c
 * @brief Test the Snowhouse-inspired assertion framework
 */

#include "../include/uft/uft_assert.h"

UFT_TEST(equality) {
    int x = 42;
    UFT_ASSERT_EQ(x, 42);
    UFT_ASSERT_NE(x, 0);
}

UFT_TEST(comparison) {
    int a = 10;
    UFT_ASSERT_GT(a, 5);
    UFT_ASSERT_GE(a, 10);
    UFT_ASSERT_LT(a, 20);
    UFT_ASSERT_LE(a, 10);
}

UFT_TEST(boolean) {
    UFT_ASSERT_TRUE(1 == 1);
    UFT_ASSERT_FALSE(1 == 0);
    UFT_ASSERT_TRUE(42);
    UFT_ASSERT_FALSE(0);
}

UFT_TEST(pointers) {
    int *p = NULL;
    int x = 5;
    int *q = &x;
    
    UFT_ASSERT_NULL(p);
    UFT_ASSERT_NOT_NULL(q);
}

UFT_TEST(strings) {
    const char *str = "Hello, World!";
    
    UFT_ASSERT_STR_EQ(str, "Hello, World!");
    UFT_ASSERT_STR_CONTAINS(str, "World");
    UFT_ASSERT_STR_STARTS_WITH(str, "Hello");
    UFT_ASSERT_STR_ENDS_WITH(str, "World!");
}

UFT_TEST(floats) {
    double pi = 3.14159;
    UFT_ASSERT_FLOAT_EQ(pi, 3.14159, 0.00001);
    UFT_ASSERT_FLOAT_EQ(pi, 3.14, 0.01);
}

UFT_TEST(memory) {
    uint8_t a[] = {1, 2, 3, 4, 5};
    uint8_t b[] = {1, 2, 3, 4, 5};
    UFT_ASSERT_MEM_EQ(a, b, 5);
}

UFT_TEST(describe_style) {
    UFT_DESCRIBE("Calculator");
    
    UFT_IT("should add numbers", {
        int result = 2 + 3;
        UFT_ASSERT_EQ(result, 5);
    });
    
    UFT_IT("should subtract numbers", {
        int result = 10 - 4;
        UFT_ASSERT_EQ(result, 6);
    });
}

int main(void) {
    UFT_TEST_SUITE_BEGIN("UFT Assertion Framework Tests");
    
    UFT_RUN_TEST(equality);
    UFT_RUN_TEST(comparison);
    UFT_RUN_TEST(boolean);
    UFT_RUN_TEST(pointers);
    UFT_RUN_TEST(strings);
    UFT_RUN_TEST(floats);
    UFT_RUN_TEST(memory);
    UFT_RUN_TEST(describe_style);
    
    UFT_TEST_SUITE_END();
}
