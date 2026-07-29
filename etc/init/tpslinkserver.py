class Module:
    daemon = True
    order = 10

    # SeedLink backbone: serve port for consumers, feed port for acquisition.
    def start_args(self, env):
        args = ["--port", env.param("slink_serve", 18000),
                "--feed-port", env.param("slink_feed", 18001)]
        inv = env.param("inventory", "config/inventory.example.json")
        if inv:
            args += ["--inventory", inv]
        return args
