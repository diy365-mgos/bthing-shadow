# bThing Shadow Library
## Overview
Mongoose-OS library for managing [bThings](https://github.com/diy365-mgos/bthing) states as a single shadow representation.
## C/C++ API Reference

### mgos_bthing_shadow_ignore
```c
bool mgos_bthing_shadow_ignore(mgos_bthing_t thing, bool ignore);
```
Excludes or includes a bThing state or disables MQTT messages for a bThing. Returns `true` on success, or `false` otherwise.

|Parameter||
|--|--|
|thing|A bThing.|
|ignore|Exclude/include flag.|
## To Do
- Implement javascript APIs for [Mongoose OS MJS](https://github.com/mongoose-os-libs/mjs).
