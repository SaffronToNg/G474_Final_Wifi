#ifndef __POWER_PROTECTION_H__
#define __POWER_PROTECTION_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "power_mode.h"

#define POWER_ERR_NONE      0x0000U
#define POWER_ERR_VIN_UVP   0x0001U
#define POWER_ERR_VIN_OVP   0x0002U
#define POWER_ERR_VOUT_UVP  0x0004U
#define POWER_ERR_VOUT_OVP  0x0008U
#define POWER_ERR_IOUT_OCP  0x0010U
#define POWER_ERR_SHORT     0x0020U

typedef struct
{
  uint16_t error_flags;
  int32_t vin_feedback;
  int32_t vout_feedback;
  uint16_t run_tick_count;
  uint8_t armed;
} PowerProtection_t;

extern volatile PowerProtection_t g_power_protection;

void PowerProtection_Reset(void);
void PowerProtection_Evaluate(void);
uint16_t PowerProtection_GetFlags(void);

#ifdef __cplusplus
}
#endif

#endif /* __POWER_PROTECTION_H__ */
