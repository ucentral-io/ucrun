#include <ucode/compiler.h>
#include <ucode/lib.h>
#include <ucode/vm.h>

#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <libubox/uloop.h>

struct ucrun {
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

struct ucrun_timeout {
	struct list_head list;
	struct ucrun *ucrun;

	struct uloop_timeout timeout;
	uc_value_t *function;
};

static inline struct ucrun*
vm_to_ucrun(uc_vm_t *vm)
{
	return container_of(vm, struct ucrun, vm);
}

extern int ucode_init(struct ucrun *ucrun, const char *file);

extern void ubus_init(struct ucrun *ucrun);
extern void ubus_deinit(struct ucrun *ucrun);
