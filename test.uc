{%
	global.ubus = {
		object: "ucrun",

		methods: {
			foo: {
				cb: function(msg) {
					printf("%s\n", msg);
					printf("fooo\n");
					return { foo: true };
				}
			}
		}
	};

	local n_runs = 0;

	function timeout() {
		printf("timeout[%d]: %d\n", n_runs, time());

		if (++n_runs >= 3) {
			printf("not scheduling new timeout\n");
			return false;
		}

		return 5000;
	}

	uloop_timeout(timeout, 1000);
%}
