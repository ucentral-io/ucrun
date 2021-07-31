/*
 * Copyright (C) 2021 Jo-Philipp Wich <jo@mein.io>
 * Copyright (C) 2021 John Crispin <john@phrozen.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <ucode/compiler.h>
#include <ucode/lib.h>
#include <ucode/vm.h>

#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <libubox/uloop.h>
#include <libubox/ulog.h>

struct ucrun {
	struct list_head timeout;

	uc_vm_t vm;
	uc_value_t *scope;
	uc_function_t *prog;

	char *ulog_identity;

	uc_value_t *ubus;
	char *ubus_name;
	struct ubus_method *ubus_method;
	struct ubus_object_type ubus_object_type;
	struct ubus_object ubus_object;
	struct ubus_auto_conn ubus_auto_conn;
};

struct ucrun_timeout {
	struct list_head list;
	struct ucrun *ucrun;

	struct uloop_timeout timeout;
	uc_value_t *function;
	uc_value_t *priv;
};

static inline struct ucrun*
vm_to_ucrun(uc_vm_t *vm)
{
	return container_of(vm, struct ucrun, vm);
}

extern int ucode_init(struct ucrun *ucrun, int argc, const char **argv);
extern void ucode_deinit(struct ucrun *ucrun);

extern void ubus_init(struct ucrun *ucrun);
extern void ubus_deinit(struct ucrun *ucrun);
