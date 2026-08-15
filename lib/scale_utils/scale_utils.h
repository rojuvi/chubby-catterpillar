#ifndef SCALE_UTILS_H
#define SCALUE_UTILS_H

typedef struct {
    int average;
    int min;
    int max;
} MeasureStatistics;

void bubbleSortAsc(int* values, int length);
MeasureStatistics getAverageCuttingOutliers(int* values, int length, int maxRange, int maxTrim);

#endif