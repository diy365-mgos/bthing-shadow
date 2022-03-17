/*
 * Copyright (c) 2021 DIY356
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MGOS_BTHING_SHADOW_H_
#define MGOS_BTHING_SHADOW_H_

#include <stdbool.h>
#include "mgos_bthing.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MGOS_BTHING_SHADOW_EVENT_BASE MGOS_EVENT_BASE('B', 'S', 'W')
#define MGOS_EV_BTHING_SHADOW_ANY MGOS_BTHING_SHADOW_EVENT_BASE
enum mgos_bthing_shadow_event {
  MGOS_EV_BTHING_SHADOW_CHANGED = MGOS_BTHING_SHADOW_EVENT_BASE,
  MGOS_EV_BTHING_SHADOW_UPDATED
};

struct mgos_bthing_shadow_state {
  mgos_bvarc_t full_shadow;
  mgos_bvarc_t delta_shadow;
  enum mgos_bthing_state_flag state_flags;
};

//bool mgos_bthing_shadow_disable(mgos_bthing_t thing);

#if MGOS_BTHING_HAVE_ACTUATORS
bool mgos_bthing_shadow_set(mgos_bvarc_t shadow);

#ifdef MGOS_BVAR_HAVE_JSON
bool mgos_bthing_shadow_json_set(const char *json, int json_len);
#endif //MGOS_BVAR_HAVE_JSON

#endif // MGOS_BTHING_HAVE_ACTUATORS

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MGOS_BTHING_SHADOW_H_ */