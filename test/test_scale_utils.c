#include <unity.h>
#include <scale_utils.h>

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_average_within_range() {
    int values[] = {1,2,2,2,2,2,2,3};

    MeasureStatistics statistics = getAverageCuttingOutliers(values, 8, 2, 1);

    TEST_ASSERT_TRUE(statistics.average == 2);
    TEST_ASSERT_TRUE(statistics.min == 1);
    TEST_ASSERT_TRUE(statistics.max == 3);
}

void test_average_with_low_outliers() {
    int values[] = {-10,0,0,0,0,0,0,3};

    MeasureStatistics statistics = getAverageCuttingOutliers(values, 8, 10, 1);

    TEST_ASSERT_TRUE(statistics.average == 0);
    TEST_ASSERT_TRUE(statistics.min == 0);
    TEST_ASSERT_TRUE(statistics.max == 0);
}

void test_average_with_high_outliers() {
    int values[] = {-1,0,0,0,0,0,0,10};

    MeasureStatistics statistics = getAverageCuttingOutliers(values, 8, 10, 1);

    TEST_ASSERT_TRUE(statistics.average == 0);
    TEST_ASSERT_TRUE(statistics.min == 0);
    TEST_ASSERT_TRUE(statistics.max == 0);
}

void test_average_with_limited_trims() {
    int values[] = {-1000,-100,-10,0,0,10,100,1000};

    MeasureStatistics statistics = getAverageCuttingOutliers(values, 8, 10, 2);

    TEST_ASSERT_TRUE(statistics.average == 0);
    TEST_ASSERT_TRUE(statistics.min == -10);
    TEST_ASSERT_TRUE(statistics.max == 10);
}

int main( int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_average_within_range);
    RUN_TEST(test_average_with_low_outliers);
    RUN_TEST(test_average_with_high_outliers);
    RUN_TEST(test_average_with_limited_trims);

    UNITY_END();
}