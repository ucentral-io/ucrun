{%
	global.ubus = {
		object: "urun",

		methods: {
			foo: {
				cb: function() {
					printf("fooo");
					return { foo: true };
				}
			}
		}
	};

	function timeout() {
		printf("timeout\n");

		return 5000;
	}

//	uloop_timeout(timeout, 1000);
%}
