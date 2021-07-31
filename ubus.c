#include "ucrun.h"

static struct blob_buf u;

struct ucrun *
ctx_to_ucrun(struct ubus_context *ctx)
{
	struct ubus_auto_conn *conn = container_of(ctx, struct ubus_auto_conn, ctx);
	struct ucrun *ucrun = container_of(conn, struct ucrun, ubus_auto_conn);

	return ucrun;
}

static int
ubus_ucode_cb(struct ubus_context *ctx,
	      struct ubus_object *obj,
	      struct ubus_request_data *req,
	      const char *name,
	      struct blob_attr *msg)
{
	struct ucrun *ucrun = ctx_to_ucrun(ctx);
	char *json = NULL;

	/* try to find the method */
	uc_value_t *methods = ucv_object_get(ucrun->ubus, "methods", NULL);
	uc_value_t *method = NULL, *cb, *retval = NULL;

	ucv_object_foreach(methods, key, val) {
		if (strcmp(key, name))
			continue;
		method = val;
	}

	if (!method)
		return UBUS_STATUS_METHOD_NOT_FOUND;

	/* check if we want to pass anything into the function */
	if (msg)
		json = blobmsg_format_json(msg, true);

	/* check if the callback is valid */
	cb = ucv_object_get(method, "cb", NULL);
	if (!ucv_is_callable(cb))
		return UBUS_STATUS_METHOD_NOT_FOUND;

	/* push the callback to the stack */
	ucv_get(cb);
	uc_vm_stack_push(&ucrun->vm, cb);
	if (json)
		uc_vm_stack_push(&ucrun->vm, ucv_string_new(json));

	/* execute the callback */
	if (!uc_vm_call(&ucrun->vm, false, json ? 1 : 0))
		retval = uc_vm_stack_pop(&ucrun->vm);
	else
		fprintf(stderr, "Failed to invoke ubus cb\n");

	if (retval) {
		blob_buf_init(&u, 0);
		blobmsg_add_json_from_string(&u, ucv_to_string(&ucrun->vm, retval));

		/* check if we need to send a reply */
		if (blobmsg_len(u.head))
			ubus_send_reply(ctx, req, u.head);
	}

	if (json)
		free(json);

	return UBUS_STATUS_OK;
}

static void
ubus_connect_handler(struct ubus_context *ctx)
{
	struct ucrun *ucrun = ctx_to_ucrun(ctx);

	ubus_add_object(ctx, &ucrun->ubus_object);
}

void
ubus_init(struct ucrun *ucrun)
{
	int n_methods, n = 0;

	/* validate that the ubus declaration is complete */
	uc_value_t *object = ucv_object_get(ucrun->ubus, "object", NULL);
	uc_value_t *methods = ucv_object_get(ucrun->ubus, "methods", NULL);

	ucv_get(ucrun->ubus);

	if (ucv_type(object) != UC_STRING || ucv_type(methods) != UC_OBJECT) {
		fprintf(stderr, "The ubus declaration is incomplete\n");
		return;
	}

	/* create our ubus methods */
	n_methods = ucv_object_length(methods);
	ucrun->ubus_method = malloc(n_methods * sizeof(struct ubus_method));
	memset(ucrun->ubus_method, 0, n_methods * sizeof(struct ubus_method));

	n_methods = 0;
	ucv_object_foreach(methods, key, val) {
		if (!ucv_object_get(val, "cb", NULL))
			continue;

		ucrun->ubus_method[n].name = key;
		ucrun->ubus_method[n].handler = ubus_ucode_cb;
		n_methods++;
	}

	/* setup the ubus object */
	ucrun->ubus_name = strdup(ucv_string_get(object));

	ucrun->ubus_object_type.name = ucrun->ubus_name;
	ucrun->ubus_object_type.methods = ucrun->ubus_method;
	ucrun->ubus_object_type.n_methods = n_methods;

	ucrun->ubus_object.name = ucrun->ubus_name;
	ucrun->ubus_object.type = &ucrun->ubus_object_type;
	ucrun->ubus_object.methods = ucrun->ubus_method;
	ucrun->ubus_object.n_methods = n_methods;

	/* try to connect to ubus */
	memset(&ucrun->ubus_auto_conn, 0, sizeof(ucrun->ubus_auto_conn));
	ucrun->ubus_auto_conn.cb = ubus_connect_handler;
        ubus_auto_connect(&ucrun->ubus_auto_conn);
}

void
ubus_deinit(struct ucrun *ucrun)
{
	if (!ucrun->ubus)
		return;

        ubus_auto_shutdown(&ucrun->ubus_auto_conn);
}
