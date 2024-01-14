/*
 * 
 * Copyright (c) 2024 Belkacem BENADDA
 *  for the Make it Matter contest hackster.io 2024
 *  nrf5340 netcore firmware
 */

#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <esb.h>
#include <zephyr/ipc/ipc_service.h>


/*
* Global part
*/
#define STACKSIZE	(1024)
#define MESSAGE_LEN 50
#define CONFIG_APP_IPC_SERVICE_SEND_INTERVAL 2000

const struct device *ipc0_instance;
struct ipc_ept ep;

K_THREAD_STACK_DEFINE(ipc0_stack, STACKSIZE);

struct Nodes_msg{
  uint8_t data_len;
  char data[CONFIG_ESB_MAX_PAYLOAD_LENGTH];
  };
 
unsigned long current_cnt=0;
unsigned long old_cnt=0;
struct payload {
	unsigned long cnt;
	char role;
	int8_t rssi;
	struct Nodes_msg data;
};

struct payload *IPC_msg;

/*
* ESB part
*/

struct Nodes_msg *R_msg;


static struct esb_payload rx_payload;
static struct esb_payload tx_payload = ESB_CREATE_PAYLOAD(0, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17);

//esb_event_handler_to_be_completed
void event_handler(struct esb_evt const *event)
{
	int ret;
	R_msg =  (struct Nodes_msg *) k_malloc(MESSAGE_LEN);
	memset(R_msg, 0, MESSAGE_LEN);
	IPC_msg = (struct payload *) k_malloc(MESSAGE_LEN);
	memset(IPC_msg, 0, MESSAGE_LEN);
	
	switch (event->evt_id) {
	case ESB_EVENT_TX_SUCCESS:
			R_msg->data[0]= 'S';
			R_msg->data[1]= 'C';
			R_msg->data[2]= 'E';
			R_msg->data[3]= 'S';
			R_msg->data[4]= 'S';
			R_msg->data_len = 5;
			IPC_msg->role = 'M'; // M sending data as master node
		break;
	case ESB_EVENT_TX_FAILED:
			R_msg->data[0]= 'F'; // Comming back later to fix the issue initialisation
			R_msg->data[1]= 'A';
			R_msg->data[2]= 'I';
			R_msg->data[3]= 'L';
			R_msg->data_len = 4;
			IPC_msg->role = 'M'; // M sending data as master node
		break;
	case ESB_EVENT_RX_RECEIVED:  
		if (esb_read_rx_payload(&rx_payload) == 0) {
			*(R_msg->data)= *(rx_payload.data);
			R_msg->data_len = rx_payload.length;
			IPC_msg->role = 'N';
		} else {
			R_msg->data[0]= 'F'; // Comming back later to fix the issue initialisation
			R_msg->data[1]= 'A';
			R_msg->data[2]= 'I';
			R_msg->data[3]= 'L';
			R_msg->data_len = 4;
			IPC_msg->role = 'N'; // Receiving data from mesh nodes
		}
		break;
	}
	
	k_free(R_msg);
	IPC_msg->cnt = current_cnt;
	old_cnt = current_cnt++;
	IPC_msg->data = *R_msg;
	IPC_msg->rssi = rx_payload.rssi;
	ret = ipc_service_send(&ep, IPC_msg, MESSAGE_LEN);
	k_free(IPC_msg);

}


int esb_initialize(void)
{
	int err;
	
	uint8_t base_addr_0[4] = {0x78, 0x78, 0x78, 0x78};
	uint8_t base_addr_1[4] = {0x84, 0x84, 0x84, 0x84};
	uint8_t addr_prefix[8] = {0x78, 0x84, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8};

	struct esb_config config = ESB_DEFAULT_CONFIG;

	config.protocol = ESB_PROTOCOL_ESB_DPL;
	config.bitrate = ESB_BITRATE_2MBPS;
	config.mode = ESB_MODE_PRX;
	config.event_handler = event_handler;
	config.selective_auto_ack = true;

	err = esb_init(&config);
	if (err) {
		return err;
	}

	err = esb_set_base_address_0(base_addr_0);
	if (err) {
		return err;
	}

	err = esb_set_base_address_1(base_addr_1);
	if (err) {
		return err;
	}

	err = esb_set_prefixes(addr_prefix, ARRAY_SIZE(addr_prefix));
	if (err) {
		return err;
	}

	return 0;
}

static K_SEM_DEFINE(bound_sem, 0, 1);

static void ep_bound(void *priv)
{
	k_sem_give(&bound_sem);
}

static void ep_recv(const void *data, size_t len, void *priv)
{
	struct payload d = *((struct payload *)data);
	esb_flush_tx();
	*(tx_payload.data) = *(u_int8_t*) d.data.data;
	tx_payload.length = d.data.data_len;
	tx_payload.noack =false;
	tx_payload.rssi = 30;
	
}

static struct ipc_ept_cfg ep_cfg = {
	.name = "ep0",
	.cb = {
		.bound    = ep_bound,
		.received = ep_recv,
	},
};

int main(void)
{
    int err;
	
    /*
	* ESB
	*/
	err = esb_initialize();
	if (err) {
		return err;
	}

	err = esb_write_payload(&tx_payload);
	if (err) {
		return err;
	}
	    
    err = esb_start_rx();
	if (err) {
		return err;
	}
/*
* IPC
*/
	ipc0_instance = DEVICE_DT_GET(DT_NODELABEL(ipc0));
	
	ipc_service_open_instance(ipc0_instance);
	if ((err < 0) && (err != -EALREADY)) {
		return err;
	}

	err = ipc_service_register_endpoint(ipc0_instance, &ep, &ep_cfg);
	if (err < 0) {
		return err;
	}

			R_msg =  (struct Nodes_msg *) k_malloc(MESSAGE_LEN);
			memset(R_msg, 0, MESSAGE_LEN);
			IPC_msg = (struct payload *) k_malloc(MESSAGE_LEN);
			memset(IPC_msg, 0, MESSAGE_LEN);
			R_msg->data[0]= 'S';
			R_msg->data[1]= 'C';
			R_msg->data[2]= 'E';
			R_msg->data[3]= 'S';
			R_msg->data[4]= 'S';
			R_msg->data_len = 5;
			IPC_msg->role = 'M'; // M sending data as master nodeIPC_msg->role = 'N';
			k_free(R_msg);
			IPC_msg->cnt = current_cnt;
			old_cnt = current_cnt++;
			IPC_msg->data = *R_msg;
			IPC_msg->rssi = rx_payload.rssi;
			k_msleep(50);
			err = ipc_service_send(&ep, IPC_msg, MESSAGE_LEN);
			IPC_msg->cnt = current_cnt++;
			k_free(IPC_msg);

    
	return 0;
}
