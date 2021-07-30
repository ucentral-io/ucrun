#include "urun.h"

static uc_parse_config_t config = {
        .strict_declarations = false,
        .lstrip_blocks = true,
        .trim_blocks = true
};

static int
ucode_run(struct urun *urun)
{
	int exit_code = 0;

	/* increase the refcount of the function */
	ucv_get(&urun->prog->header);

	/* execute compiled program function */
	uc_value_t *last_expression_result = NULL;
	int return_code = uc_vm_execute(&urun->vm, urun->prog, &last_expression_result);

	/* handle return status */
	switch (return_code) {
	case STATUS_OK:
		exit_code = 0;

		char *s = ucv_to_string(&urun->vm, last_expression_result);

		//printf("Program finished successfully.\n");
		//printf("Function return value is %s\n", s);
		break;

	case STATUS_EXIT:
		exit_code = (int)ucv_int64_get(last_expression_result);

		printf("The invoked program called exit().\n");
		printf("Exit code is %d\n", exit_code);
		break;

	case ERROR_COMPILE:
		exit_code = -1;

		printf("A compilation error occurred while running the program\n");
		break;

	case ERROR_RUNTIME:
		exit_code = -2;

		printf("A runtime error occurred while running the program\n");
		break;
	}

	/* call the garbage collector */
	//ucv_gc(&urun->vm);

	return exit_code;
}

static uc_function_t*
ucode_load(const char *file) {
	/* create a source buffer from the given input file */
	uc_source_t *src = uc_source_new_file(file);

	/* check if source file could be opened */
	if (!src) {
		fprintf(stderr, "Unable to open source file %s\n", file);
		return NULL;
	}

	/* compile source buffer into function */
	char *syntax_error = NULL;
	uc_function_t *progfunc = uc_compile(&config, src, &syntax_error);

	/* release source buffer */
	uc_source_put(src);

	/* check if compilation failed */
	if (!progfunc)
		fprintf(stderr, "Failed to compile program: %s\n", syntax_error);

	return progfunc;
}

static void
uc_uloop_timeout_cb(struct uloop_timeout *t)
{
	struct urun_timeout *timeout = container_of(t, struct urun_timeout, timeout);
	uc_value_t *retval = NULL;

	/* push the function to the stack and execute it */
	ucv_get(timeout->function);
	uc_vm_stack_push(&timeout->urun->vm, timeout->function);
	if (!uc_vm_call(&timeout->urun->vm, false, 0))
		retval = uc_vm_stack_pop(&timeout->urun->vm);
fprintf(stderr, "%s:%s[%d]\n", __FILE__, __func__, __LINE__);

	/* this returns 10 instead of 500 */
	if (retval && ucv_type(retval) == UC_INTEGER)
		fprintf(stderr, "%s:%s[%d]%ld\n", __FILE__, __func__, __LINE__, ucv_int64_get(retval));

	/* if the callback returned an integer, restart the timer */
	if (0 && retval && ucv_type(retval) == UC_INTEGER) {
		uloop_timeout_set(&timeout->timeout, ucv_int64_get(retval));
		return;
	}

	/* free the timer context */
	ucv_put(timeout->function);
	free(timeout);
}

static uc_value_t *
uc_uloop_timeout(uc_vm_t *vm, size_t nargs)
{
	struct urun_timeout *timeout;

	uc_value_t *function = uc_fn_arg(0);
	uc_value_t *expire = uc_fn_arg(1);

	/* check if the call signature is correct */
	if (!ucv_is_callable(function) || ucv_type(expire) != UC_INTEGER)
		return ucv_int64_new(-1);

	/* add the uloop timer */
	timeout = malloc(sizeof(*timeout));
	memset(timeout, 0, sizeof(*timeout));
	timeout->function = ucv_get(function);
	timeout->timeout.cb = uc_uloop_timeout_cb;
	timeout->urun = vm_to_urun(vm);
	uloop_timeout_set(&timeout->timeout, ucv_int64_get(expire));

	return ucv_int64_new(0);
}

static void
ucode_init_ubus(struct urun *urun)
{
	uc_value_t *ubus = ucv_object_get(uc_vm_scope_get(&urun->vm), "ubus", NULL);

	if (!ubus)
		return;

	urun->ubus = ucv_get(ubus);
	ubus_init(urun);
}

int
ucode_init(struct urun *urun, const char *file)
{
	/* initialize VM context */
	uc_vm_init(&urun->vm, &config);

	/* load our user code */
	urun->prog = ucode_load(file);
	if (!urun->prog) {
		uc_vm_free(&urun->vm);
		return -1;
	}

	/* load standard library into global VM scope */
	uc_stdlib_load(uc_vm_scope_get(&urun->vm));
	uc_function_register(uc_vm_scope_get(&urun->vm), "uloop_timeout", uc_uloop_timeout);

	/* load our user code */
	ucode_run(urun);

	/* spawn ubus if requested */
	ucode_init_ubus(urun);

	return 0;
}
