#ifndef UTILS_H
#define UTILS_H

int scale(int value, int input_min, int input_max, int scale_min, int scale_max)
{
  return (((float)value - input_min) / (input_max - input_min) * (scale_max - scale_min)) + scale_min;
}

#endif