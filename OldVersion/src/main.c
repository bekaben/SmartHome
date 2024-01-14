/*
 * 
 * Copyright (c) 2024 Belkacem BENADDA
 *  for the Make it Matter contest hackster.io 2024
 * 
 */

/*
* Libraries part
*/

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
// #include <zephyr/drivers/mbox.h>
#include <zephyr/ipc/ipc_service.h>

//Loger activation
LOG_MODULE_REGISTER(Beka_Home,LOG_LEVEL_INF);

/*
* Definition part
*/
#define LEDPRIORITY 10
#define STACKSIZE 1024

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)

/*
* IPC
*/

#define MESSAGE_LEN 50

bool blink_led0;
struct Nodes_msg{
  uint8_t data_len;
  char data[32];
  };

struct esbpayload {
	unsigned long cnt;
	char role;
	int8_t rssi;
	struct Nodes_msg data;
};

struct esbpayload *payload;
K_THREAD_STACK_DEFINE(ipc0_stack, STACKSIZE);
static K_SEM_DEFINE(bound_sem, 0, 1);

static void ep_bound(void *priv)
{
	k_sem_give(&bound_sem);
}

static void ep_recv(const void *data, size_t len, void *priv)
{
	struct esbpayload d = *((struct esbpayload *)data);
	
	LOG_INF("message received id: %ld , Role : %c, content size : %d, data : %s",	d.cnt,d.role,d.data.data_len, d.data.data);

	if(d.cnt==0) {
		LOG_INF("Led0 start blinking");
		blink_led0 = true;
	}
}

static struct ipc_ept_cfg ep_cfg = {
	.name = "ep0",
	.cb = {
		.bound    = ep_bound,
		.received = ep_recv,
	},
};

void IPC_initialize(){

}
/*
* Wifi Part
*/


/*
* Led parts
*/
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);

int led_initialize(const struct gpio_dt_spec *led){
	int ret;

	if (!gpio_is_ready_dt(led)) {
		LOG_ERR("Error: %s device is not ready\n", led->port->name);
		return -1; 
	}

	ret = gpio_pin_configure_dt(led, GPIO_OUTPUT);
	if (ret != 0) {
		LOG_ERR("Code %d: failed to configure LED pin %d \n", ret, led->pin);
		return -1;
	}
	return 0;
}
void blink(const struct gpio_dt_spec *led, uint32_t sleep_ms, bool *s)
{
	while (1) {
		if(*s){
			gpio_pin_toggle_dt(led);
		}else{
			gpio_pin_set_dt(led,0);
		}
		k_msleep(sleep_ms);
	}
}

void blink0(void)
{
	uint16_t T = 500;
	LOG_INF("Starting blinking Thread for LED0 at %d ms \n", T);
	blink(&led0, T, &blink_led0);
}

void blink1(void)
{
	uint16_t T = 1000;
	LOG_INF("Starting blinking Thread for LED1 at %d ms \n", T);
    bool b = true;
	blink(&led1, T, &b);
}

// General Configuration
int main(void)
{
	LOG_INF("Starting Application on : %s", CONFIG_BOARD);
	LOG_INF("LED1 Configuration \n");
	led_initialize(&led1);
	blink_led0= false;
	LOG_INF("LED0 Configuration \n");
	led_initialize(&led0);	

	LOG_INF("IPC Configuration");
	IPC_initialize();
	const struct device *ipc0_instance;
	struct ipc_ept ep;
	int ret;
	
	ipc0_instance = DEVICE_DT_GET(DT_NODELABEL(ipc0));

	ret = ipc_service_open_instance(ipc0_instance);
	if ((ret < 0) && (ret != -EALREADY)) {
		LOG_INF("ipc_service_open_instance() failure");
		return ret;
	}

	ret = ipc_service_register_endpoint(ipc0_instance, &ep, &ep_cfg);
	if (ret < 0) {
		LOG_INF("ipc_service_register_endpoint() failure");
		return ret;
	}

	k_sem_take(&bound_sem, K_FOREVER);
	return 0;
}

// Treads definition
K_THREAD_DEFINE(blink0_id, STACKSIZE, blink0, NULL, NULL, NULL,	LEDPRIORITY, 0, 0);
K_THREAD_DEFINE(blink1_id, STACKSIZE, blink1, NULL, NULL, NULL, LEDPRIORITY, 0, 0);

