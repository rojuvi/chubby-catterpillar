#include "scale_utils.h"

MeasureStatistics getAverageCuttingOutliers(int* values, int length, int maxRange, int maxTrim) {
  int startI = 0;
  int endI = length-1;
  bubbleSortAsc(values, length);
  int range = values[endI] - values[startI];
  int trims = 0;
  while (trims < maxTrim && range > maxRange) {
    startI++;
    endI--;
    range = values[endI] - values[startI];
    if (range < 0) {
      range = -range;
    }
    trims++;
  }
  int sum = 0;
  for (int i=startI;i<=endI;i++) {
    sum += values[i];
  }
  int average = sum / (endI - startI + 1);
  MeasureStatistics statistics = {average, values[startI], values[endI]};
  return statistics;
}

void bubbleSortAsc(int* values, int length)
{
   int i, j, flag = 1;
   int temp;
   for (i = 1; (i <= length) && flag; i++)
   {
      flag = 0;
      for (j = 0; j < (length - 1); j++)
      {
         if (values[j + 1] < values[j])
         {
            temp = values[j];
            values[j] = values[j + 1];
            values[j + 1] = temp;
            flag = 1;
         }
      }
   }
}