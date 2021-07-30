#include "urun.h"

static struct blob_buf u;

struct urun *
ctx_to_urun(struct ubus_context *ctx)
{
	struct ubus_auto_conn *conn = container_of(ctx, struct ubus_auto_conn, ctx);
	struct urun *urun = container_of(conn, struct urun, ubus_auto_conn);

	return urun;
}

static int
ubus_ucode_cb(struct ubus_context *ctx,
	      struct ubus_object *obj,
	      struct ubus_request_data *req,
	      const char *name,
	      struct blob_attr *msg)
{
	struct urun *urun = ctx_to_urun(ctx);
	char *json = NULL;

	/* try to find the method */
	uc_value_t *methods = ucv_object_get(urun->ubus, "methods", NULL);
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
	uc_vm_stack_push(&urun->vm, cb);
	if (json)
		uc_vm_stack_push(&urun->vm, ucv_string_new(json));

	/* execute the callback */
	if (!uc_vm_call(&urun->vm, false, json ? 1 : 0))
		retval = uc_vm_stack_pop(&urun->vm);
	else
		fprintf(stderr, "Failed to invoke ubus cb\n");

	if (ucv_type(retval) == UC_STRING) {
		blob_buf_init(&u, 0);
		blobmsg_add_json_from_string(&u, ucv_string_get(retval));

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
	struct urun *urun = ctx_to_urun(ctx);

	ubus_add_object(ctx, &urun->ubus_object);
}

void
ubus_init(struct urun *urun)
{
	int n_methods, n = 0;

	/* validate that the ubus declaration is complete */
	uc_value_t *object = ucv_object_get(urun->ubus, "object", NULL);
	uc_value_t *methods = ucv_object_get(urun->ubus, "methods", NULL);

	ucv_get(urun->ubus);

	if (ucv_type(object) != UC_STRING || ucv_type(methods) != UC_OBJECT) {
		fprintf(stderr, "The ubus declaration is incomplete\n");
		return;
	}

	/* create our ubus methods */
	n_methods = ucv_object_length(methods);
	urun->ubus_method = malloc(n_methods * sizeof(struct ubus_method));
	memset(urun->ubus_method, 0, n_methods * sizeof(struct ubus_method));

	n_methods = 0;
	ucv_object_foreach(methods, key, val) {
		if (!ucv_object_get(val, "cb", NULL))
			continue;

		urun->ubus_method[n].name = key;
		urun->ubus_method[n].handler = ubus_ucode_cb;
		n_methods++;
	}

	/* setup the ubus object */
	urun->ubus_name = strdup(ucv_string_get(object));

	urun->ubus_object_type.name = urun->ubus_name;
	urun->ubus_object_type.methods = urun->ubus_method;
	urun->ubus_object_type.n_methods = n_methods;

	urun->ubus_object.name = urun->ubus_name;
	urun->ubus_object.type = &urun->ubus_object_type;
	urun->ubus_object.methods = urun->ubus_method;
	urun->ubus_object.n_methods = n_methods;

	/* try to connect to ubus */
	memset(&urun->ubus_auto_conn, 0, sizeof(urun->ubus_auto_conn));
	urun->ubus_auto_conn.cb = ubus_connect_handler;
        ubus_auto_connect(&urun->ubus_auto_conn);
}

void
ubus_deinit(struct urun *urun)
{
	if (!urun->ubus)
		return;

        ubus_auto_shutdown(&urun->ubus_auto_conn);
}
