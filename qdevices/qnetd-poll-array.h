/*
 * Copyright (c) 2015 Red Hat, Inc.
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

#ifndef _QNETD_POLL_ARRAY_H_
#define _QNETD_POLL_ARRAY_H_

#include <sys/types.h>
#include <inttypes.h>

#include <nspr.h>

#include "qnetd-client.h"
#include "qnetd-clients-list.h"

#ifdef __cplusplus
extern "C" {
#endif

struct qnetd_poll_array {
	PRPollDesc *array;
	unsigned int allocated;
	unsigned int items;
};


extern void		 qnetd_poll_array_init(struct qnetd_poll_array *poll_array);

extern void		 qnetd_poll_array_destroy(struct qnetd_poll_array *poll_array);

extern void		 qnetd_poll_array_clean(struct qnetd_poll_array *poll_array);

extern unsigned int	 qnetd_poll_array_size(struct qnetd_poll_array *poll_array);

extern PRPollDesc	*qnetd_poll_array_add(struct qnetd_poll_array *poll_array);

extern PRPollDesc 	*qnetd_poll_array_get(const struct qnetd_poll_array *poll_array, unsigned int pos);

extern PRPollDesc	*qnetd_poll_array_create_from_clients_list(struct qnetd_poll_array *poll_array,
    const struct qnetd_clients_list *clients_list, PRFileDesc *extra_fd, PRInt16 extra_fd_in_flags);

#ifdef __cplusplus
}
#endif

#endif /* _QNETD_POLL_ARRAY_H_ */
