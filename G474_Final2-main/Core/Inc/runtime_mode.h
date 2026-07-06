#ifndef __RUNTIME_MODE_H__
#define __RUNTIME_MODE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef enum
{
  RUNTIME_MODE_NONE = 0,
  RUNTIME_MODE_BUCK = 1,
  RUNTIME_MODE_BOOST = 2
} RuntimeMode_t;

extern volatile RuntimeMode_t g_runtime_mode;

void RuntimeMode_Reset(void);
void RuntimeMode_Set(RuntimeMode_t mode);
RuntimeMode_t RuntimeMode_Get(void);

#ifdef __cplusplus
}
#endif

#endif /* __RUNTIME_MODE_H__ */
