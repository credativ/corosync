/*
 * Copyright (c) 2015-2016 Red Hat, Inc.
 *
 * All rights reserved.
 *
 * Author: Jan Friesse (jfriesse@redhat.com)
 *
 * This software licensed under BSD license, the text of which follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the Red Hat, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdint.h>
#include <netdb.h>
#include <err.h>
#include <poll.h>

/*
 * Needed for creating nspr handle from unix fd
 */
#include <private/pprio.h>

#include "qnet-config.h"
#include "qdevice-net-cmap.h"
#include "qdevice-net-log.h"
#include "qdevice-net-send.h"

static uint32_t
qdevice_net_cmap_autogenerate_node_id(const char *addr, int clear_node_high_byte)
{
	struct addrinfo *ainfo;
	struct addrinfo ahints;
	int ret, i;

	memset(&ahints, 0, sizeof(ahints));
	ahints.ai_socktype = SOCK_DGRAM;
	ahints.ai_protocol = IPPROTO_UDP;
	/*
	 * Hardcoded AF_INET because autogenerated nodeid is valid only for ipv4
	 */
	ahints.ai_family   = AF_INET;

	ret = getaddrinfo(addr, NULL, &ahints, &ainfo);
	if (ret != 0)
		return (0);

	if (ainfo->ai_family != AF_INET) {

		freeaddrinfo(ainfo);

		return (0);
	}

        memcpy(&i, &((struct sockaddr_in *)ainfo->ai_addr)->sin_addr, sizeof(struct in_addr));
	freeaddrinfo(ainfo);

	ret = htonl(i);

	if (clear_node_high_byte) {
		ret &= 0x7FFFFFFF;
	}

	return (ret);
}

int
qdevice_net_cmap_get_nodelist(cmap_handle_t cmap_handle, struct node_list *list)
{
	cs_error_t cs_err;
	cmap_iter_handle_t iter_handle;
	char key_name[CMAP_KEYNAME_MAXLEN + 1];
	char tmp_key[CMAP_KEYNAME_MAXLEN + 1];
	int res;
	int ret_value;
	unsigned int node_pos;
	uint32_t node_id;
	uint32_t data_center_id;
	char *tmp_str;
	char *addr0_str;
	int clear_node_high_byte;

	ret_value = 0;

	node_list_init(list);

	cs_err = cmap_iter_init(cmap_handle, "nodelist.node.", &iter_handle);
	if (cs_err != CS_OK) {
		return (-1);
	}

	while ((cs_err = cmap_iter_next(cmap_handle, iter_handle, key_name, NULL, NULL)) == CS_OK) {
		res = sscanf(key_name, "nodelist.node.%u.%s", &node_pos, tmp_key);
		if (res != 2) {
			continue;
		}

		if (strcmp(tmp_key, "ring0_addr") != 0) {
			continue;
		}

		snprintf(tmp_key, CMAP_KEYNAME_MAXLEN, "nodelist.node.%u.nodeid", node_pos);
		cs_err = cmap_get_uint32(cmap_handle, tmp_key, &node_id);

		if (cs_err == CS_ERR_NOT_EXIST) {
			/*
			 * Nodeid doesn't exists -> autogenerate node id
			 */
			clear_node_high_byte = 0;

			if (cmap_get_string(cmap_handle, "totem.clear_node_high_bit",
			    &tmp_str) == CS_OK) {
				if (strcmp (tmp_str, "yes") == 0) {
					clear_node_high_byte = 1;
				}

				free(tmp_str);
			}

			if (cmap_get_string(cmap_handle, key_name, &addr0_str) != CS_OK) {
				return (-1);
			}

			node_id = qdevice_net_cmap_autogenerate_node_id(addr0_str,
			    clear_node_high_byte);

			free(addr0_str);
		} else if (cs_err != CS_OK) {
			ret_value = -1;

			goto iter_finalize;
		}

		snprintf(tmp_key, CMAP_KEYNAME_MAXLEN, "nodelist.node.%u.datacenterid", node_pos);
		if (cmap_get_uint32(cmap_handle, tmp_key, &data_center_id) != CS_OK) {
			data_center_id = 0;
		}

		if (node_list_add(list, node_id, data_center_id, TLV_NODE_STATE_NOT_SET) == NULL) {
			ret_value = -1;

			goto iter_finalize;
		}
	}

iter_finalize:
	cmap_iter_finalize(cmap_handle, iter_handle);

	if (ret_value != 0) {
		node_list_free(list);
	}

	return (ret_value);
}

int
qdevice_net_cmap_get_config_version(cmap_handle_t cmap_handle, uint64_t *config_version)
{
	int res;

	if (cmap_get_uint64(cmap_handle, "totem.config_version", config_version) == CS_OK) {
		res = 1;
	} else {
		*config_version = 0;
		res = 0;
	}

	return (res);
}

void
qdevice_net_cmap_init(cmap_handle_t *handle)
{
	cs_error_t res;
	int no_retries;

	no_retries = 0;

	while ((res = cmap_initialize(handle)) == CS_ERR_TRY_AGAIN &&
	    no_retries++ < QDEVICE_NET_MAX_CS_TRY_AGAIN) {
		poll(NULL, 0, 1000);
	}

        if (res != CS_OK) {
		errx(1, "Failed to initialize the cmap API. Error %s", cs_strerror(res));
	}
}

void
qdevice_net_cmap_init_fd(struct qdevice_net_instance *instance)
{
	int fd;
	cs_error_t res;

	if ((res = cmap_context_set(instance->cmap_handle, (void *)instance)) != CS_OK) {
		qdevice_net_log(LOG_ERR, "Can't set cmap context. Error %s", cs_strerror(res));
		exit(1);
	}

	cmap_fd_get(instance->cmap_handle, &fd);
	if ((instance->cmap_poll_fd = PR_CreateSocketPollFd(fd)) == NULL) {
		qdevice_net_log_nss(LOG_CRIT, "Can't create NSPR cmap poll fd");
		exit(1);
	}
}

static void
qdevice_net_cmap_nodelist_reload_cb(cmap_handle_t cmap_handle, cmap_track_handle_t cmap_track_handle,
    int32_t event, const char *key_name,
    struct cmap_notify_value new_value, struct cmap_notify_value old_value,
    void *user_data)
{
	cs_error_t cs_res;
	uint8_t reload;
	struct qdevice_net_instance *instance;

	if (cmap_context_get(cmap_handle, (const void **)&instance) != CS_OK) {
		qdevice_net_log(LOG_ERR, "Fatal error. Can't get cmap context");
		exit(1);
	}

	/*
	 * Wait for full reload
	 */
	if (strcmp(key_name, "config.totemconfig_reload_in_progress") == 0 &&
	    new_value.type == CMAP_VALUETYPE_UINT8 && new_value.len == sizeof(reload)) {
		reload = 1;
		if (memcmp(new_value.data, &reload, sizeof(reload)) == 0) {
			/*
			 * Ignore nodelist changes
			 */
			instance->cmap_reload_in_progress = 1;
			return ;
		} else {
			instance->cmap_reload_in_progress = 0;
		}
	}

	if (instance->cmap_reload_in_progress) {
		return ;
	}

	if (((cs_res = cmap_get_uint8(cmap_handle, "config.totemconfig_reload_in_progress",
	    &reload)) == CS_OK) && reload == 1) {
		return ;
	}

	if (qdevice_net_send_config_node_list(instance, 0, 0) != 0) {
		/*
		 * Fatal error -> schedule disconnect
		 */
		instance->disconnect_reason = QDEVICE_NET_DISCONNECT_REASON_CANT_ALLOCATE_MSG_BUFFER;
		instance->schedule_disconnect = 1;
	}
}

int
qdevice_net_cmap_add_track(struct qdevice_net_instance *instance)
{
	cs_error_t res;

	res = cmap_track_add(instance->cmap_handle, "config.totemconfig_reload_in_progress",
	    CMAP_TRACK_ADD | CMAP_TRACK_MODIFY, qdevice_net_cmap_nodelist_reload_cb,
	    NULL, &instance->cmap_reload_track_handle);

	if (res != CS_OK) {
		qdevice_net_log(LOG_ERR, "Can't initialize cmap totemconfig_reload_in_progress tracking");
		return (-1);
	}

	res = cmap_track_add(instance->cmap_handle, "nodelist.",
	    CMAP_TRACK_ADD | CMAP_TRACK_DELETE | CMAP_TRACK_MODIFY | CMAP_TRACK_PREFIX,
	    qdevice_net_cmap_nodelist_reload_cb,
	    NULL, &instance->cmap_nodelist_track_handle);

	if (res != CS_OK) {
		qdevice_net_log(LOG_ERR, "Can't initialize cmap nodelist tracking");
		return (-1);
	}

	return (0);
}

int
qdevice_net_cmap_del_track(struct qdevice_net_instance *instance)
{
	cs_error_t res;

	res = cmap_track_delete(instance->cmap_handle, instance->cmap_reload_track_handle);

	if (res != CS_OK) {
		qdevice_net_log(LOG_WARNING, "Can't delete cmap totemconfig_reload_in_progress tracking");
	}
	instance->cmap_reload_track_handle = 0;

	res = cmap_track_delete(instance->cmap_handle, instance->cmap_nodelist_track_handle);
	instance->cmap_nodelist_track_handle = 0;

	if (res != CS_OK) {
		qdevice_net_log(LOG_WARNING, "Can't delete cmap nodelist tracking");
	}

	return (0);
}

void
qdevice_net_cmap_destroy(struct qdevice_net_instance *instance)
{
	cs_error_t res;

	res = cmap_finalize(instance->cmap_handle);

        if (res != CS_OK) {
		qdevice_net_log(LOG_WARNING, "Can't finalize cmap. Error %s", cs_strerror(res));
	}

	if (PR_DestroySocketPollFd(instance->cmap_poll_fd) != PR_SUCCESS) {
		qdevice_net_log_nss(LOG_WARNING, "Unable to close votequorum connection fd");
	}
}
