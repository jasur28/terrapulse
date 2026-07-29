class Module:
    daemon = True
    order = 30

    # Strong-motion parameters off the SeedLink backbone; optional station partition.
    def start_args(self, env):
        args = ["--slink", "%s:%s" % (env.param("center_host", "127.0.0.1"),
                                      env.param("slink_serve", 18000))]
        inv = env.param("inventory", "config/inventory.example.json")
        if inv:
            args += ["--inventory", inv]
        stations = env.param("stations", "")
        if stations:
            args += ["--stations", stations]
        return args
