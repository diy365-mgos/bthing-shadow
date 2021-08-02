#include "mgos.h"
#include "mgos_bthing_shadow.h"
#include "mgos_bvar_json.h"
#include "mgos_bvar_dic.h"
#include "mg_bthing_sdk.h"

#ifdef MGOS_HAVE_MJS
#include "mjs.h"
#endif

static struct mg_bthing_shadow_ctx {
  struct mgos_bthing_shadow_state state;
  int optimize_timer_id;
  int optimize_timeout; 
  int64_t last_change; 
} s_ctx;

static void mg_bthing_shadow_on_created(int ev, void *ev_data, void *userdata) {
  if (ev == MGOS_EV_BTHING_CREATED) {
    #if MGOS_BTHING_HAVE_SENSORS
    const char *key = mgos_bthing_get_id((mgos_bthing_t)ev_data);
    if (!mgos_bvar_add_key((mgos_bvar_t)s_ctx.state.full_shadow, key, (mgos_bvar_t)mg_bthing_get_raw_state((mgos_bthing_t)ev_data))) {
      LOG(LL_ERROR, ("Error including '%s' state to the full shadow.", key));
    }
    #else
    (void) ev_data;
    #endif
  }
  
  (void) userdata;
}

#if MGOS_BTHING_HAVE_SENSORS

static void mg_bthing_shadow_trigger_changed_event(bool force) {
  if (force || (s_ctx.last_change != 0 && 
      (mg_bthing_duration_micro(s_ctx.last_change, mgos_uptime_micros()) / 1000) >= s_ctx.optimize_timeout)) {
    // raise the SHADOW_CHANGED event
    mgos_event_trigger(MGOS_EV_BTHING_SHADOW_CHANGED, &s_ctx.state);
    // remove all keys from delta shadow
    mgos_bvar_remove_keys((mgos_bvar_t)s_ctx.state.delta_shadow);
    s_ctx.last_change = 0;
  }
}

static void mg_bthing_shadow_on_state_changing(int ev, void *ev_data, void *userdata) {
  struct mgos_bthing_state_changing_arg *arg = (struct mgos_bthing_state_changing_arg *)ev_data;
  if (mgos_bvar_has_key(s_ctx.state.delta_shadow, mgos_bthing_get_id(arg->thing))) {
    // the changed state was already queued into delta-shadow, so
    // I must flush the queue and raise SHADOW-CHANGED event
    // before moving on
    LOG(LL_INFO, ("DOUBLE CHANGE DETECTED, NOTIFYING..."));
    mg_bthing_shadow_trigger_changed_event(true);
    LOG(LL_INFO, ("DOUBLE CHANGE NOTIFYING DONE."));
  }
}

static void mg_bthing_shadow_on_state_changed(int ev, void *ev_data, void *userdata) {  
  struct mgos_bthing_state_changed_arg *arg = (struct mgos_bthing_state_changed_arg *)ev_data;
  const char *key = mgos_bthing_get_id(arg->thing);

  if (!mgos_bvar_has_key(s_ctx.state.full_shadow, key)) {
    return; // the thing must be ignored
  }

  s_ctx.last_change = mgos_uptime_micros();

  // add the changed state to the delta shadow
  if (!mgos_bvar_add_key((mgos_bvar_t)s_ctx.state.delta_shadow, key, (mgos_bvar_t)arg->state)) {
    LOG(LL_ERROR, ("Error adding '%s' state to the delta shadow.", key));
  }

  if (s_ctx.optimize_timer_id == MGOS_INVALID_TIMER_ID) {
    // raise the SHADOW_CHANGED event here because
    // the optimization is turned off
    mg_bthing_shadow_trigger_changed_event(true);
  }

  (void) userdata;
  (void) ev;
}

static void mg_bthing_shadow_changed_trigger_cb(void *arg) {
  mg_bthing_shadow_trigger_changed_event(false);
  (void) arg;
}

#endif //MGOS_BTHING_HAVE_SENSORS

bool mgos_bthing_shadow_disable(mgos_bthing_t thing) {
  const char *key = mgos_bthing_get_id(thing);
  if (mgos_bvar_has_key(s_ctx.state.full_shadow, key)) {
    if (mgos_bvar_remove_key((mgos_bvar_t)s_ctx.state.full_shadow, key) == NULL) {
      LOG(LL_ERROR, ("Error excluding '%s' state from the full shadow.", key));
      return false;
    }
  }
  return true;
}

#if MGOS_BTHING_HAVE_ACTUATORS

bool mgos_bthing_shadow_set(mgos_bvarc_t shadow) {
  if (shadow && mgos_bvar_is_dic(shadow)) {
    const char *thing_id;
    mgos_bvarc_t key_val;
    mgos_bvarc_enum_t keys = mgos_bvarc_get_keys(shadow);
    while (mgos_bvarc_get_next_key(&keys, &key_val, &thing_id)) {
      if (mgos_bvar_has_key(s_ctx.state.full_shadow, thing_id)) {
        mgos_bthing_t thing = mgos_bthing_get(thing_id);
        mgos_bthing_set_state(thing, key_val);
      }
    }
    return true;
  }
  LOG(LL_ERROR, ("Invalid shadow. A bVariandDictionary is expected."));
  return false;
}

#ifdef MGOS_BVAR_HAVE_JSON

bool mgos_bthing_shadow_json_set(const char *json, int json_len){
  mgos_bvar_t shadow = NULL;
  if (mgos_bvar_json_try_bscanf(json, json_len, &shadow)) {
    bool ret = mgos_bthing_shadow_set(shadow);
    mgos_bvar_free(shadow);
    return ret;
  }
  LOG(LL_ERROR, ("Invalid json shadow format. A json object is expected."));
  return false;
}

#endif // MGOS_BVAR_HAVE_JSON
#endif //MGOS_BTHING_HAVE_ACTUATORS

bool mgos_bthing_shadow_init() {

  if (!mgos_sys_config_get_bthing_shadow_enable()) return true;

  // init context
  s_ctx.state.full_shadow = mgos_bvar_new_dic();
  s_ctx.state.delta_shadow = mgos_bvar_new_dic();
  s_ctx.optimize_timer_id = MGOS_INVALID_TIMER_ID;
  s_ctx.last_change = 0;
  s_ctx.optimize_timeout = mgos_sys_config_get_bthing_shadow_optimize_timeout();

  if (!mgos_event_register_base(MGOS_BTHING_SHADOW_EVENT_BASE, "bThing Shadow events")) return false;

   if (!mgos_event_add_handler(MGOS_EV_BTHING_CREATED, mg_bthing_shadow_on_created, NULL)) {
    LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_CREATED handler."));
    return false;
  }
  
  #if MGOS_BTHING_HAVE_SENSORS
  if (!mgos_event_add_handler(MGOS_EV_BTHING_STATE_CHANGING, mg_bthing_shadow_on_state_changing, NULL)) {
    LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_STATE_CHANGING handler."));
    return false;
  }

  if (!mgos_event_add_handler(MGOS_EV_BTHING_STATE_CHANGED, mg_bthing_shadow_on_state_changed, NULL)) {
    LOG(LL_ERROR, ("Error registering MGOS_EV_BTHING_STATE_CHANGED handler."));
    return false;
  }
  
  if (mgos_sys_config_get_bthing_shadow_optimize() && (s_ctx.optimize_timeout > 0)) {
    s_ctx.optimize_timer_id = mgos_set_timer(s_ctx.optimize_timeout,
      MGOS_TIMER_REPEAT, mg_bthing_shadow_changed_trigger_cb, NULL);
    if (s_ctx.optimize_timer_id == MGOS_INVALID_TIMER_ID) {
      LOG(LL_DEBUG, ("Warning: unable to start the Shadow Optimizer."));
    } else {
      LOG(LL_DEBUG, ("Shadow Optimizer successfully stared (timeout %dms).", s_ctx.optimize_timeout));
    }
  } else {
    LOG(LL_DEBUG, ("Shadow Optimizer disabled."));
  }
  #endif //MGOS_BTHING_HAVE_SENSORS

  return true;
}