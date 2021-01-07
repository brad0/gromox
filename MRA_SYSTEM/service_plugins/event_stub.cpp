// SPDX-License-Identifier: GPL-2.0-only WITH linking exception
#include <stdbool.h>
#include <libHX/string.h>
#include <gromox/fileio.h>
#include <gromox/socket.h>
#include <gromox/svc_common.h>
#include "double_list.h"
#include "config_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>  
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <poll.h>


#define SOCKET_TIMEOUT          60

#define MAX_CMD_LENGTH			64*1024

struct BACK_CONN {
    DOUBLE_LIST_NODE node;
	pthread_t thr_id;
    int sockd;
};

typedef void (*EVENT_STUB_FUNC)(char *);

static BOOL g_notify_stop;
static char g_event_ip[32];
static int g_event_port;
static DOUBLE_LIST g_back_list;
static EVENT_STUB_FUNC g_event_stub_func;


static void* thread_work_func(void *param);

static int read_line(int sockd, char *buff, int length);
static int connect_event(void);
static void install_event_stub(EVENT_STUB_FUNC event_stub_func);

DECLARE_API;

BOOL SVC_LibMain(int reason, void **ppdata)
{
	int i, conn_num;
    BACK_CONN *pback;
	CONFIG_FILE *pfile;
	char file_name[256];
	char config_path[256];
	DOUBLE_LIST_NODE *pnode;
	char *str_value, *psearch;
	
	switch(reason) {
	case PLUGIN_INIT: {
		LINK_API(ppdata);
		
		g_notify_stop = TRUE;
		g_event_stub_func = NULL;
		double_list_init(&g_back_list);

		/* get the plugin name from system api */
		strcpy(file_name, get_plugin_name());
		psearch = strrchr(file_name, '.');
		if (NULL != psearch) {
			*psearch = '\0';
		}
		sprintf(config_path, "%s/%s.cfg", get_config_path(), file_name);
		pfile = config_file_init2(NULL, config_path);
		if (NULL == pfile) {
			printf("[event_proxy]: config_file_init %s: %s\n", config_path, strerror(errno));
			return FALSE;
		}

		str_value = config_file_get_value(pfile, "CONNECTION_NUM");
		if (NULL == str_value) {
			conn_num = 8;
			config_file_set_value(pfile, "CONNECTION_NUM", "8");
		} else {
			conn_num = atoi(str_value);
			if (conn_num < 0) {
				conn_num = 8;
				config_file_set_value(pfile, "CONNECTION_NUM", "8");
			}
		}
		printf("[event_proxy]: event connection number is %d\n", conn_num);

		str_value = config_file_get_value(pfile, "EVENT_HOST");
		if (NULL == str_value) {
			strcpy(g_event_ip, "127.0.0.1");
			config_file_set_value(pfile, "EVENT_HOST", "127.0.0.1");
		} else {
			strcpy(g_event_ip, str_value);
		}
		printf("[event_proxy]: event host is %s\n", g_event_ip);

		str_value = config_file_get_value(pfile, "EVENT_PORT");
		if (NULL == str_value) {
			g_event_port = 33333;
			config_file_set_value(pfile, "EVENT_PORT", "33333");
		} else {
			g_event_port = atoi(str_value);
			if (g_event_port <= 0) {
				g_event_port = 33333;
				config_file_set_value(pfile, "EVENT_PORT", "33333");
			}
		}
		printf("[event_proxy]: event port is %d\n", g_event_port);
		config_file_free(pfile);

		g_notify_stop = FALSE;
		int ret = 0;
		for (i=0; i<conn_num; i++) {
			pback = (BACK_CONN*)malloc(sizeof(BACK_CONN));
			if (NULL != pback) {
		        pback->node.pdata = pback;
				pback->sockd = -1;
				ret = pthread_create(&pback->thr_id, nullptr, thread_work_func, pback);
				if (ret != 0) {
					free(pback);
					break;
				}
				char buf[32];
				snprintf(buf, sizeof(buf), "event_stub/%u", i);
				pthread_setname_np(pback->thr_id, buf);
				double_list_append_as_tail(&g_back_list, &pback->node);
			}
		}

		if (i < conn_num) {
			g_notify_stop = TRUE;
			while ((pnode = double_list_get_from_head(&g_back_list)) != NULL) {
				pback = (BACK_CONN*)pnode->pdata;
				if (-1 != pback->sockd) {
					close(pback->sockd);
					pback->sockd = -1;
				}
				pthread_join(pback->thr_id, NULL);
				free(pback);
			}
			double_list_free(&g_back_list);
			printf("[event_proxy]: failed to create stub thread: %s\n", strerror(ret));
			return FALSE;
		}

		if (!register_service("install_event_stub",
		    reinterpret_cast<void *>(install_event_stub)))
			printf("[event_proxy]: failed to register install_event_stub\n");
		return TRUE;
	}
	case PLUGIN_FREE:
		if (FALSE == g_notify_stop) {
			g_notify_stop = TRUE;
		}
		double_list_free(&g_back_list);
		g_event_stub_func = NULL;
		return TRUE;
	}
	return false;
}


static int read_line(int sockd, char *buff, int length)
{
	int offset;
	int tv_usec;
	int read_len;
	struct pollfd pfd_read;

	offset = 0;
	while (1) {
		tv_usec = SOCKET_TIMEOUT * 1000000;
		pfd_read.fd = sockd;
		pfd_read.events = POLLIN|POLLPRI;
		if (1 != poll(&pfd_read, 1, tv_usec)) {
			return -1;
		}
		read_len = read(sockd, buff + offset, length - offset);
		if (read_len <= 0) {
			return -1;
		}
		offset += read_len;
		if (offset >= 2 &&
			'\r' == buff[offset - 2] && '\n' == buff[offset - 1]) {
			buff[offset - 2] = '\0';
			return 0;
		}
		if (length == offset) {
			return -1;
		}
	}
}


static int connect_event()
{
	int temp_len;
    char temp_buff[1024];
	int sockd = gx_inet_connect(g_event_ip, g_event_port, 0);
	if (sockd < 0)
		return -1;
	if (-1 == read_line(sockd, temp_buff, 1024) ||
		0 != strcasecmp(temp_buff, "OK")) {
        close(sockd);
        return -1;
	}
	
	temp_len = gx_snprintf(temp_buff, GX_ARRAY_SIZE(temp_buff), "LISTEN %s:%d\r\n",
				get_host_ID(), getpid());
	if (temp_len != write(sockd, temp_buff, temp_len)) {
		close(sockd);
		return -1;
	}

	if (-1 == read_line(sockd, temp_buff, 1024) ||
		0 != strcasecmp(temp_buff, "TRUE")) {
		close(sockd);
		return -1;
	}

	return sockd;
}


static void* thread_work_func(void *param)
{
	BACK_CONN *pback;
	char buff[MAX_CMD_LENGTH];	
	
	pback = (BACK_CONN*)param;

	while (FALSE == g_notify_stop) {
		if (-1 == (pback->sockd = connect_event())) {
			sleep(3);
			continue;
		}

		while (FALSE == g_notify_stop) {
			if (-1 == read_line(pback->sockd, buff, MAX_CMD_LENGTH)) {
				close(pback->sockd);
				pback->sockd = -1;
				break;
			}
		
			if (0 == strcasecmp(buff, "PING")) {
				write(pback->sockd, "TRUE\r\n", 6);
				continue;
			}

			if (NULL != g_event_stub_func) {
				g_event_stub_func(buff);
			}
			
			write(pback->sockd, "TRUE\r\n", 6);
		}
	}
	
	if (-1 != pback->sockd) {
		close(pback->sockd);
		pback->sockd = -1;
	}
	free(pback);
	pthread_exit(0);
}


static void install_event_stub(EVENT_STUB_FUNC event_stub_func)
{
	if (NULL == g_event_stub_func) {
		g_event_stub_func = event_stub_func;
	}
}

