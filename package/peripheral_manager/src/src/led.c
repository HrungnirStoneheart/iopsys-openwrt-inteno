#include <syslog.h>
#include "log.h"
#include "led.h"

static struct blob_buf bblob;

typedef enum {
    LED_OFF,
    LED_OK,
    LED_NOTICE,
    LED_ALERT,
    LED_ERROR,
    LED_CUSTOM,
    LED_ACTION_MAX,
} led_action_t;

typedef enum {
    LEDS_NORMAL,
    LEDS_PROXIMITY,
    LEDS_SILENT,
    LEDS_INFO,
    LEDS_TEST,
    LEDS_PROD,
    LEDS_RESET,
    LEDS_ALLON,
    LEDS_ALLOFF,
    LEDS_MAX,
} leds_state_t;

/* Names for led_action_t */
static const char * const fn_actions[LED_ACTION_MAX] =
{ "off", "ok", "notice", "alert", "error", "custom"};

#define LED_FUNCTIONS 14
static const char* const led_functions[LED_FUNCTIONS] =
{ "dsl", "wifi", "wps", "lan", "status", "dect", "tv", "usb",
  "wan", "internet", "voice1", "voice2", "eco", "gbe"};

/* Names for led_state_t */
static const char* const led_states[LED_STATES_MAX] =
{ "off", "on", "flash_slow", "flash_fast","breading", "fadeon", "fadeoff" };

/* Names for leds_state_t */
static const char* const leds_states[LEDS_MAX] =
{ "normal", "proximity", "silent", "info", "test", "production", "reset", "allon" , "alloff"};

/* lowest level, contain states, timers,pointer to driver for a single physical led.*/
struct led {
	struct list_head list;
	led_state_t state;		/* state that this led should have, set from the config file */
	struct led_drv *drv;
};

/*middle layer contains lists of leds /buttons/... that should be set to a specific state */
struct function_action {
	const char *name;		/* If name is set this led action is in use by the board. */
	struct list_head led_list;
	struct list_head button_list;
};

/* main struct for the function leds.*/
struct function_led {
	const char *name;		/* If name is set this led function is in use by the board.        */
	led_action_t state;		/* state of the function led. contain what action is currently set */
	struct function_action actions[LED_ACTION_MAX];
};

struct function_led leds[LED_FUNCTIONS];

static leds_state_t global_state;	/* global state for the leds,overrids individual states */

int get_index_by_name(const char *const*array, int max, const char *name);
struct led_drv *get_drv_led(char *name);
static void dump_drv_list(void);
static void dump_led(void);
static void all_leds_off(void);
static void all_leds_on(void);
static void all_leds(led_state_t state);

/* we find out the index for a match in an array of char pointers containing max number of pointers */
int get_index_by_name(const char *const*array, int max, const char *name)
{
	int i;
	for (i=0; i < max ; i++ ){
		if (!strcasecmp(name, array[i]))
			return i;
	}
	return -1;
}

/* PUT every led from drivers into a list */
struct drv_led_list{
	struct list_head list;
	struct led_drv *drv;
};
LIST_HEAD(drv_leds_list);

void led_add( struct led_drv *drv)
{
	struct drv_led_list *drv_node = malloc(sizeof(struct drv_led_list));

	DBG(1,"called with led name [%s]\n", drv->name);
	drv_node->drv = drv;

	list_add(&drv_node->list, &drv_leds_list);
}

static void all_leds(led_state_t state) {
	struct drv_led_list *node;
	DBG(1, "set to state %d\n",state);

	list_for_each_entry(node, &drv_leds_list, list) {
		node->drv->func->set_state( node->drv, state);
	}
}

static void all_leds_off(void) {
	all_leds(OFF);
}

static void all_leds_on(void) {
	all_leds(ON);
}

#define TEST_TIMEOUT 250
static void test_handler(struct uloop_timeout *timeout);
static struct uloop_timeout test_inform_timer = { .cb = test_handler };

static void test_handler(struct uloop_timeout *timeout) {

	static int cnt = 0;
	static led_state_t state = OFF;

	static struct drv_led_list *led;
	DBG(1,"cnt = %d state %d\n",cnt,state);

	/* flash all leads 2 times.*/
	if ( cnt < 4) {
		cnt++;
		if (state == OFF){
			all_leds_on();
			state = ON;
		}else{
			all_leds_off();
			state = OFF;
		}
		goto done;
	}

	if (global_state == LEDS_RESET){
		cnt = 0;
		goto done;
	}

	/* cycle through every led once */
	if (cnt == 4 ) {
		cnt++;
		led = list_first_entry(&drv_leds_list, struct drv_led_list, list );
	}
	if (cnt == 5 ) {
		if (state == OFF){
			led->drv->func->set_state(led->drv, ON);
			state = ON;
		}else{
			led->drv->func->set_state(led->drv, OFF);
			/* was this the last led ? if so stop */
			if ( list_is_last(&led->list, &drv_leds_list) ){
				cnt = 0;
				state = OFF;
				goto done;
			}else{ /* nope more leds in list. get next and turn it on */
				led = (struct drv_led_list *)led->list.next;
				led->drv->func->set_state(led->drv, ON);
				state = ON;
			}
		}
	}
done:

    if (global_state == LEDS_TEST || global_state == LEDS_RESET)
	    uloop_timeout_set(&test_inform_timer, TEST_TIMEOUT);
    else{
	    cnt = 0;
	    state = OFF;
    }
}

/* go over the driver list for any led name that matches name and returna pointer to driver. */
struct led_drv *get_drv_led(char *name)
{
	struct list_head *i;
	list_for_each(i, &drv_leds_list) {
		struct drv_led_list *node = list_entry(i, struct drv_led_list, list);
		if (!strcmp(node->drv->name, name))
			return node->drv;
	}
	return NULL;
}

static void dump_drv_list(void)
{
	struct list_head *i;
	list_for_each(i, &drv_leds_list) {
		struct drv_led_list *node = list_entry(i, struct drv_led_list, list);
		DBG(1,"led name = [%s]\n",node->drv->name);
	}
}

static void dump_led(void)
{
	int i,j;

	for (i = 0; i < LED_FUNCTIONS ; i++) {
		for (j = 0 ; j < LED_ACTION_MAX; j++ ) {
			if ( leds[i].actions[j].name != NULL ) {
				struct led *led;
				list_for_each_entry(led, &leds[i].actions[j].led_list, list) {
					DBG(2,"%-15s %-8s %-15s %-10s\n",
					    leds[i].name,
					    leds[i].actions[j].name,
					    led->drv->name,
					    led_states[led->state]);
}}}}}

static void set_function_led(const char* fn_name, const char* action) {
	int led_idx = get_index_by_name(led_functions, LED_FUNCTIONS , fn_name);
	int act_idx = get_index_by_name(fn_actions   , LED_ACTION_MAX, action );

	if(led_idx == -1) {
		syslog(LOG_WARNING, "called over ubus with non valid led name [%s]\n", fn_name);
		return;
	}
	if(act_idx == -1) {
		syslog(LOG_WARNING, "called over ubus with non valid action [%s] for led [%s]\n", action, fn_name);
		return;
	}

	leds[led_idx].state = act_idx;

	if ( leds[led_idx].actions[act_idx].name != NULL ) {
		struct led *led;
		list_for_each_entry(led, &leds[led_idx].actions[act_idx].led_list, list) {
			if (led->drv){
				led->drv->func->set_state(led->drv, led->state);
			}
		}
	}
	syslog(LOG_WARNING, "led set on [%s] has no valid [%s] handler registered.\n", fn_name, action);
}

enum {
	LED_STATE,
	__LED_MAX
};

static const struct blobmsg_policy led_policy[] = {
	[LED_STATE] = { .name = "state", .type = BLOBMSG_TYPE_STRING },
};

static int led_set_method(struct ubus_context *ubus_ctx, struct ubus_object *obj,
                          struct ubus_request_data *req, const char *method,
                          struct blob_attr *msg)
{
	struct blob_attr *tb[__LED_MAX];
	char* state;

	DBG(1,"led_set_method (%s)\n",method);

	blobmsg_parse(led_policy, ARRAY_SIZE(led_policy), tb, blob_data(msg), blob_len(msg));

	if (tb[LED_STATE]) {
		char *fn_name = strchr(obj->name, '.') + 1;
		state = blobmsg_data(tb[LED_STATE]);
//		syslog(LOG_INFO, "Led %s method: %s state %s", fn_name, method, state);
		set_function_led(fn_name, state);
	}

	return 0;
}

static int led_status_method(struct ubus_context *ubus_ctx, struct ubus_object *obj,
                             struct ubus_request_data *req, const char *method,
                             struct blob_attr *msg)
{
	char *fn_name = strchr(obj->name, '.') + 1;
	int led_idx = get_index_by_name(led_functions, LED_FUNCTIONS , fn_name);
	DBG(1,"for led %s\n",leds[led_idx].name);

	blob_buf_init (&bblob, 0);
	blobmsg_add_string(&bblob, "state",fn_actions[leds[led_idx].state]);
	ubus_send_reply(ubus_ctx, req, bblob.head);

	return 0;
}

static int leds_set_method(struct ubus_context *ubus_ctx, struct ubus_object *obj,
                           struct ubus_request_data *req, const char *method,
                           struct blob_attr *msg)
{
	struct blob_attr *tb[__LED_MAX];
	DBG(1,"\n");

	blobmsg_parse(led_policy, ARRAY_SIZE(led_policy), tb, blob_data(msg), blob_len(msg));

	if (tb[LED_STATE]) {
		char* state;
		int state_idx;

		state = blobmsg_data(tb[LED_STATE]);
		state_idx = get_index_by_name(leds_states, LEDS_MAX , state);

		if (state_idx == -1) {
			syslog(LOG_WARNING, "leds_set_method: Unknown state %s.\n", state);
			return 0;
		}

		global_state = state_idx;

		if (global_state == LEDS_INFO) {
			all_leds_off();
			set_function_led("eco", "off"); // KEN ??? what is this ?
			set_function_led("eco", "ok");
		}

		if (global_state == LEDS_PROD) {
			all_leds_off();
		}

		if (global_state == LEDS_TEST || global_state == LEDS_RESET) {
			all_leds_off();
			uloop_timeout_set(&test_inform_timer, TEST_TIMEOUT);
		}
		if (global_state == LEDS_ALLON) {
			all_leds_on();
		}
		if (global_state == LEDS_ALLOFF) {
			all_leds_off();
		}
	}else
		syslog(LOG_WARNING, "leds_set_method: Unknown attribute.\n");

	return 0;
}

static int leds_status_method(struct ubus_context *ubus_ctx, struct ubus_object *obj,
                              struct ubus_request_data *req, const char *method,
                              struct blob_attr *msg)
{
	DBG(1,"\n");

	blob_buf_init (&bblob, 0);
//	blobmsg_add_string(&bblob, "state", leds_states[led_cfg->leds_state]);
	blobmsg_add_string(&bblob, "state", "leds_fixme_state");
	ubus_send_reply(ubus_ctx, req, bblob.head);
	return 0;
}

static const struct ubus_method led_methods[] = {
	UBUS_METHOD("set", led_set_method, led_policy),
	{ .name = "status", .handler = led_status_method },
};

static struct ubus_object_type led_object_type =
	UBUS_OBJECT_TYPE("led", led_methods);



static const struct ubus_method leds_methods[] = {
	UBUS_METHOD("set", leds_set_method, led_policy),
	{ .name = "status", .handler = leds_status_method },
//    { .name = "proximity", .handler = leds_proximity_method },
};

static struct ubus_object_type leds_object_type =
	UBUS_OBJECT_TYPE("leds", leds_methods);


#define LED_OBJECTS 15
static struct ubus_object led_objects[LED_OBJECTS] = {
    { .name = "leds",	        .type = &leds_object_type, .methods = leds_methods, .n_methods = ARRAY_SIZE(leds_methods), },
    { .name = "led.dsl",	.type = &led_object_type, .methods = led_methods, .n_methods = ARRAY_SIZE(led_methods), },
    { .name = "led.wifi",	.type = &led_object_type, .methods = led_methods, .n_methods = ARRAY_SIZE(led_methods), },
    { .name = "led.wps",	.type = &led_object_type, .methods = led_methods, .n_methods = ARRAY_SIZE(led_methods), },
    { .name = "led.lan",	.type = &led_object_type, .methods = led_methods, .n_methods = ARRAY_SIZE(led_methods), },
    { .name = "led.status",	.type = &led_object_type, .methods = led_methods, .n_methods = ARRAY_SIZE(led_methods), },
    { .name = "led.dect",	.type = &led_object_type, .methods = led_methods, .n_methods = ARRAY_SIZE(led_methods), },
    { .name = "led.tv",		.type = &led_object_type, .methods = led_methods, .n_methods = ARRAY_SIZE(led_methods), },
    { .name = "led.usb",	.type = &led_object_type, .methods = led_methods, .n_methods = ARRAY_SIZE(led_methods), },
    { .name = "led.wan",	.type = &led_object_type, .methods = led_methods, .n_methods = ARRAY_SIZE(led_methods), },
    { .name = "led.internet",	.type = &led_object_type, .methods = led_methods, .n_methods = ARRAY_SIZE(led_methods), },
    { .name = "led.voice1",	.type = &led_object_type, .methods = led_methods, .n_methods = ARRAY_SIZE(led_methods), },
    { .name = "led.voice2",	.type = &led_object_type, .methods = led_methods, .n_methods = ARRAY_SIZE(led_methods), },
    { .name = "led.eco",	.type = &led_object_type, .methods = led_methods, .n_methods = ARRAY_SIZE(led_methods), },
    { .name = "led.gbe",	.type = &led_object_type, .methods = led_methods, .n_methods = ARRAY_SIZE(led_methods), },
};


#define FLASH_TIMEOUT 250
static void flash_handler(struct uloop_timeout *timeout);
static struct uloop_timeout flash_inform_timer = { .cb = flash_handler };

static void flash_handler(struct uloop_timeout *timeout)
{
	static int counter = 1; /* bit 0 is fast flash bit 1 is slow flash */
	int i;
	led_state_t slow=OFF,fast=OFF;
	counter++;

	if (counter & 1 )
		fast = ON;
	if (counter & 2 )
		slow = ON;

	if (global_state == LEDS_NORMAL) {
		/* BUG we should check if the driver support flash in hardware and only do this on simple on/off leds */
		for (i = 0; i < LED_FUNCTIONS ; i++) {
			struct led *led;
			list_for_each_entry(led, &leds[i].actions[leds[i].state].led_list, list) {
				if (led->state == FLASH_FAST){
					if (led->drv)
						led->drv->func->set_state(led->drv, fast);
				}else if (led->state == FLASH_SLOW){
					if (led->drv)
						led->drv->func->set_state(led->drv, slow);
				}
			}
		}
	}
	uloop_timeout_set(&flash_inform_timer, FLASH_TIMEOUT);
}

void led_init( struct server_ctx *s_ctx)
{
	int i,j,ret;
//	char *led_names;
	DBG(1,"\n");

	/* map */

	dump_drv_list();
	/* register leds with ubus */

	for (i=0 ; i<LED_OBJECTS ; i++) {
		ret = ubus_add_object(s_ctx->ubus_ctx, &led_objects[i]);
		if (ret)
			DBG(1,"Failed to add object: %s\n", ubus_strerror(ret));
	}

	/* we create a top list of led functions */
	/* that list contains a new list of actions */
	/* every action contains led actions lists */
	/* the led states is attached to the drv_leds lists */

//	led_names = ucix_get_option(s_ctx->uci_ctx, "hw", "board", "lednames");

	for (i = 0; i < LED_FUNCTIONS ; i++) {
		for (j = 0 ; j < LED_ACTION_MAX; j++ ) {
			char fn_name_action[256];

			LIST_HEAD(led_action_list);
			struct ucilist *node;

			snprintf(fn_name_action, 256, "%s_%s", led_functions[i], fn_actions[j]);
			ucix_get_option_list( s_ctx->uci_ctx, "hw", fn_name_action, "led_action" , &led_action_list);

			INIT_LIST_HEAD( &leds[i].actions[j].led_list );

			if (!list_empty(&led_action_list)) {

				/* Found led with action, init structs */
				leds[i].name  = led_functions[i];
				leds[i].state = LED_OFF;

				leds[i].actions[j].name    = fn_actions[j];

				/* fill in led actions */
				DBG(1,"%-15s has led actions -> \n",fn_name_action);
				list_for_each_entry(node, &led_action_list, list) {
					char led_name[256],led_state[256];
					struct led *led;
					struct led_drv *drv;

					/* get pointer to low level led driver.*/
					sscanf(node->val, "%[^=]=%s", led_name, led_state);
					drv = get_drv_led(led_name);

					if (drv) {
						led = malloc(sizeof(struct led));
						led->drv = drv;
						led->state = get_index_by_name(led_states, LED_STATES_MAX, led_state);
						list_add(&led->list, &leds[i].actions[j].led_list);
					}else {
						syslog(LOG_ERR,"Config specified use of led name [%s]. But it's not registerd with a led driver.\n", led_name);
					}
					DBG(1, "%-35s%s\n","",node->val);
				}

				/* fill in button actions */

				/* fill in xxx actions */
			}
		}
	}

	uloop_timeout_set(&flash_inform_timer, FLASH_TIMEOUT);

	dump_led();
}
