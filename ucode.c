#include "ucrun.h"

static uc_parse_config_t config = {
        .strict_declarations = false,
        .lstrip_blocks = true,
        .trim_blocks = true
};

static int
ucode_run(struct ucrun *ucrun)
{
	int exit_code = 0;

	/* increase the refcount of the function */
	ucv_get(&ucrun->prog->header);

	/* execute compiled program function */
	uc_value_t *last_expression_result = NULL;
	int return_code = uc_vm_execute(&ucrun->vm, ucrun->prog, &last_expression_result);

	/* handle return status */
	switch (return_code) {
	case STATUS_OK:
		exit_code = 0;

		//char *s = ucv_to_string(&ucrun->vm, last_expression_result);
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
	//ucv_gc(&ucrun->vm);

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
	struct ucrun_timeout *timeout = container_of(t, struct ucrun_timeout, timeout);
	uc_value_t *retval = NULL;

	/* push the function to the stack */
	uc_vm_stack_push(&timeout->ucrun->vm, ucv_get(timeout->function));

	/* invoke function with zero arguments */
	if (uc_vm_call(&timeout->ucrun->vm, false, 0)) {
		/* function raised an exception, bail out */
		goto out;
	}

	retval = uc_vm_stack_pop(&timeout->ucrun->vm);

	/* if the callback returned an integer, restart the timer */
	if (ucv_type(retval) == UC_INTEGER) {
		uloop_timeout_set(&timeout->timeout, ucv_int64_get(retval));
		return;
	}

out:
	/* free the timer context */
	ucv_put(timeout->function);
	free(timeout);
}

static uc_value_t *
uc_uloop_timeout(uc_vm_t *vm, size_t nargs)
{
	struct ucrun_timeout *timeout;

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
	timeout->ucrun = vm_to_ucrun(vm);
	uloop_timeout_set(&timeout->timeout, ucv_int64_get(expire));

	return ucv_int64_new(0);
}

static void
ucode_init_ubus(struct ucrun *ucrun)
{
	uc_value_t *ubus = ucv_object_get(uc_vm_scope_get(&ucrun->vm), "ubus", NULL);

	if (!ubus)
		return;

	ucrun->ubus = ucv_get(ubus);
	ubus_init(ucrun);
}

int
ucode_init(struct ucrun *ucrun, const char *file)
{
	/* initialize VM context */
	uc_vm_init(&ucrun->vm, &config);

	/* load our user code */
	ucrun->prog = ucode_load(file);
	if (!ucrun->prog) {
		uc_vm_free(&ucrun->vm);
		return -1;
	}

	/* load standard library into global VM scope */
	uc_stdlib_load(uc_vm_scope_get(&ucrun->vm));
	uc_function_register(uc_vm_scope_get(&ucrun->vm), "uloop_timeout", uc_uloop_timeout);

	/* load our user code */
	ucode_run(ucrun);

	/* spawn ubus if requested */
	ucode_init_ubus(ucrun);

	return 0;
}
