#include "runtime_mode.h"

volatile RuntimeMode_t g_runtime_mode = RUNTIME_MODE_NONE;

void RuntimeMode_Reset(void)
{
  g_runtime_mode = RUNTIME_MODE_NONE;
}

void RuntimeMode_Set(RuntimeMode_t mode)
{
  g_runtime_mode = mode;
}

RuntimeMode_t RuntimeMode_Get(void)
{
  return g_runtime_mode;
}
