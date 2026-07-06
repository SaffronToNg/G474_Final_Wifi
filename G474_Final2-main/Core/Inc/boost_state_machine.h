#ifndef __BOOST_STATE_MACHINE_H__
#define __BOOST_STATE_MACHINE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef enum
{
  BOOST_STATE_INIT = 0,
  BOOST_STATE_WAIT = 1,
  BOOST_STATE_RISE = 2,
  BOOST_STATE_RUN = 3,
  BOOST_STATE_ERR = 4
} BoostState_t;

typedef struct
{
  BoostState_t state;
  uint16_t wait_counter;
  uint16_t rise_counter;
  uint8_t pwm_enabled;
  uint8_t output_enabled_request;
} BoostStateMachine_t;

extern volatile BoostStateMachine_t g_boost_state_machine;

void BoostStateMachine_Init(void);
void BoostStateMachine_Tick5ms(void);
void BoostStateMachine_RunControlTick(void);
const char *BoostStateMachine_StateName(BoostState_t state);

#ifdef __cplusplus
}
#endif

#endif /* __BOOST_STATE_MACHINE_H__ */
