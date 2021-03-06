# bThings Shadow Library
## Overview
Mongoose-OS library for managing [bThings](https://github.com/diy365-mgos/bthing) states as a single shadow state representation. The shadow is a [bVariantDictionary](https://github.com/diy365-mgos/bvar-dic) collecting the states of all registered bThings. Each state is added into the dictionary using the bThing ID as key. NOTE: private bThings (see [mgos_bthing_make_private()](https://github.com/diy365-mgos/bthing#mgos_bthing_make_private) function) won't be exposed.
## Features
- **Observable** - You can detect when one or more states of the shadow change subscribing to its [events](#mgos_bthing_shadow_event).
- **Optimized** - You can enable the shadow optimization to prevent several sequential changes from being notified. When optimization is active the library trys to collect as much changes/updates as possible triggering one single [event](#mgos_bthing_shadow_event).
## Configuration
The library adds the `bthing.shadow` section to the device configuration:
```javascript
{
  "enable": true,           // Enable or disable the shadow
  "optimize": true,         // Optimize shadow collecting us much changes as possibles in one single change
  "optimize_timeout": 100,  // The timeout, in milliseconds, to use for collecting changes
}
```
## C/C++ API Reference
### mgos_bthing_shadow_event
```c
enum mgos_bthing_shadow_event {
  MGOS_EV_BTHING_SHADOW_CHANGED,
  MGOS_EV_BTHING_SHADOW_PUBLISHING
};
```
Events triggered by a shadow state. Use [mgos_event_add_handler()](https://mongoose-os.com/docs/mongoose-os/api/core/mgos_event.h.md#mgos_event_add_handler) or [mgos_event_add_group_handler(MGOS_EV_BTHING_SHADOW_ANY, ...)](https://mongoose-os.com/docs/mongoose-os/api/core/mgos_event.h.md#mgos_event_add_group_handler) for subscribing to them.

|Event||
|--|--|
|MGOS_EV_BTHING_SHADOW_CHANGED|Triggered when the shadow state is changed. The event-data passed to the handler is a `struct mgos_bthing_shadow_state*`.<br><br>Allowed `state_flags` are: `MGOS_BTHING_STATE_FLAG_CHANGED`, `MGOS_BTHING_STATE_FLAG_PUBLISHING` and optionally `MGOS_BTHING_STATE_FLAG_FORCED_PUBLISH`.|
|MGOS_EV_BTHING_SHADOW_PUBLISHING|Triggered when the shadow state is changed or the publish has been forcibly requested invoking [mgos_bthing_update_state(..., true)](https://github.com/diy365-mgos/bthing#mgos_bthing_update_state) or [mgos_bthing_update_states(true)](https://github.com/diy365-mgos/bthing#mgos_bthing_update_states). The event-data passed to the handler is a `struct mgos_bthing_shadow_state*`.<br><br>This event is not triggered if all changed/updated bThings are private instances (see [mgos_bthing_make_private()](#mgos_bthing_make_private) function).<br><br>Allowed `state_flags` are: `MGOS_BTHING_STATE_FLAG_PUBLISHING` and optionally `MGOS_BTHING_STATE_FLAG_CHANGED`, `MGOS_BTHING_STATE_FLAG_FORCED_PUBLISH`.|
### mgos_bthing_shadow_state
```c
struct mgos_bthing_shadow_state {
  mgos_bvarc_t full_shadow;
  mgos_bvarc_t delta_shadow;
  enum mgos_bthing_state_flag state_flags;
};
```
Event-data passed to `MGOS_EV_BTHING_SHADOW_CHANGED` and `MGOS_EV_BTHING_SHADOW_PUBLISHING` event's handlers (see [mgos_event_handler_t](https://mongoose-os.com/docs/mongoose-os/api/core/mgos_event.h.md#mgos_event_handler_t)).

|Field||
|--|--|
|full_shadow|A [bVariantDictionary](https://github.com/diy365-mgos/bvar-dic) containing all states.|
|delta_shadow|A [bVariantDictionary](https://github.com/diy365-mgos/bvar-dic) containing only changed states.|
|state_flags|Shadow state flags. It could be a combination of one or more [mgos_bthing_state_flag](https://github.com/diy365-mgos/bthing#mgos_bthing_state_flag) flags depending on the triggered [event](#mgos_bthing_shadow_event).<br><br>Note: `MGOS_BTHING_STATE_FLAG_INITIALIZING`, `MGOS_BTHING_STATE_FLAG_CHANGING` and `MGOS_BTHING_STATE_FLAG_INITIALIZED` are not used.|
### mgos_bthing_shadow_set
```c
bool mgos_bthing_shadow_set(mgos_bvarc_t shadow);
```
Sets bThing states using the provided shadow. Returns `true` on success, or `false` otherwise. This function is available only `#if MGOS_BTHING_HAVE_ACTUATORS`.

|Parameter||
|--|--|
|shadow|A [bVariantDictionary](https://github.com/diy365-mgos/bvar-dic) containing states to set.|
### mgos_bthing_shadow_json_set
```c
bool mgos_bthing_shadow_json_set(const char *json, int json_len);
```
Sets bThing states using the provided JSON shadow representation. Returns `true` on success, or `false` otherwise. This function is available only including the [Variant JSON library](https://github.com/diy365-mgos/bvar-json) and `#if MGOS_BTHING_HAVE_ACTUATORS`. 

|Parameter||
|--|--|
|json|The shadow representation in JSON format.|
|json_len|The JSON buffer length.|
## To Do
- Implement javascript APIs for [Mongoose OS MJS](https://github.com/mongoose-os-libs/mjs).