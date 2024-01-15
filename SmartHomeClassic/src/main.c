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
#include <zephyr/ipc/ipc_service.h>
#include <zephyr/types.h>
#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <net/wifi_mgmt_ext.h>
#include <net/wifi_credentials.h>
#include <zephyr/net/socket.h>

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
* Wifi Part
*/
#define EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
static struct net_mgmt_event_callback mgmt_cb;
static bool connected;
static K_SEM_DEFINE(run_app, 0, 1);
static int sock;
static struct sockaddr_storage server;
//static uint8_t recv_buf[33];

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
				   struct net_if *iface)
{
	if ((mgmt_event & EVENT_MASK) != mgmt_event) {
		return;
	}

	if (mgmt_event == NET_EVENT_L4_CONNECTED) {
		LOG_INF("Network connected");
		connected = true;
		
		k_sem_give(&run_app);
		return;
	}

	if (mgmt_event == NET_EVENT_L4_DISCONNECTED) {
		if (connected == false) {
			LOG_INF("Waiting for network to be connected");
		} else {
			
			LOG_INF("Network disconnected");
			connected = false;
		}
		k_sem_reset(&run_app);
		return;
	}
}

static int server_connect(void)
{
	int err;
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		LOG_INF("Failed to create socket, err: %d, %s", errno, strerror(errno));
		return -errno;
	}
	struct sockaddr_in MyPC;
	MyPC.sin_port =htons(1976);
	MyPC.sin_family=AF_INET;
	err = inet_pton(AF_INET,"192.168.1.107",&(MyPC.sin_addr));
	if (err <=0) {
		LOG_INF("Fail to setup server IP adresse");
		return -errno;
	}
	struct sockaddr_in *tmp = ((struct sockaddr_in *)&server);
	*tmp = MyPC;

	err = connect(sock, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
	if (err < 0) {
		LOG_INF("Connecting to server failed, err: %d, %s", errno, strerror(errno));
		return -errno;
	}
	LOG_INF("Successfully connected to server");

	return 0;
}

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
	struct Nodes_msg rec_msg;
	rec_msg = d.data;
	char *send_buf = ((char *)&rec_msg);
	int err = send(sock, send_buf, sizeof(rec_msg), 0);
		if (err < 0) {
			LOG_INF("Failed to send message, %d", errno);
		}
		LOG_INF("Successfully sent message to network : %s", send_buf);

}

static struct ipc_ept_cfg ep_cfg = {
	.name = "ep0",
	.cb = {
		.bound    = ep_bound,
		.received = ep_recv,
	},
};

int IPC_initialize(){
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
	return 0;
}

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
    
	blink(&led1, T, &connected);
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
	
	k_sem_take(&bound_sem, K_FOREVER);
	
	/*
	* WiFi
	*/
	net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler, EVENT_MASK);
	net_mgmt_add_event_callback(&mgmt_cb);

	LOG_INF("Waiting to connect to Wi-Fi");
	k_sem_take(&run_app, K_FOREVER);
	if (server_connect() != 0) {
		LOG_INF("Failed to initialize client");
		return 0;
	}

	struct Nodes_msg tostart;
	tostart.data[0]='W';
	tostart.data[1]='i';
	tostart.data[2]='F';
	tostart.data[3]='i';
	tostart.data[4]='S';
	tostart.data[5]='t';
	tostart.data[6]='a';
	tostart.data[7]='r';
	tostart.data[8]='t';
	tostart.data[9]='e';
	tostart.data[10]='d';
	tostart.data_len = sizeof(tostart.data);
	char *send_buf = ((char *)&tostart);
	int err = send(sock, send_buf, sizeof(tostart), 0);
		if (err < 0) {
			LOG_INF("Failed to send message, %d", errno);
			return err;
		}
		LOG_INF("Successfully sent message to network : %s", send_buf);

	return 0;
}

// Treads definition
K_THREAD_DEFINE(blink0_id, STACKSIZE, blink0, NULL, NULL, NULL,	LEDPRIORITY, 0, 0);
K_THREAD_DEFINE(blink1_id, STACKSIZE, blink1, NULL, NULL, NULL, LEDPRIORITY, 0, 0);

