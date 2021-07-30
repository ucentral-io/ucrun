#include <ucode/compiler.h>
#include <ucode/lib.h>
#include <ucode/vm.h>

#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <libubox/uloop.h>

struct urun {
	struct list_head timeout;

	uc_vm_t vm;
	uc_function_t *prog;


	uc_value_t *ubus;
	char *ubus_name;
	struct ubus_method *ubus_method;
	struct ubus_object_type ubus_object_type;
	struct ubus_object ubus_object;
	struct ubus_auto_conn ubus_auto_conn;
};

struct urun_timeout {
	struct list_head list;
	struct urun *urun;

	struct uloop_timeout timeout;
	uc_value_t *function;
};

static inline struct urun*
vm_to_urun(uc_vm_t *vm)
{
	return container_of(vm, struct urun, vm);
}

#include "ucode.h"
#include "ubus.h"
