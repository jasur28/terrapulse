class Module:
    daemon = True
    order = 30

    # Read waveforms from the SeedLink backbone (centre serve port); optionally
    # restrict to this instance's station partition.
    def start_args(self, env):
        args = ["--slink", "%s:%s" % (env.param("backbone_host", "127.0.0.1"),
                                      env.param("slink_serve", 18000))]
        inv = env.param("inventory", "config/inventory.example.json")
        if inv:
            args += ["--inventory", inv]
        stations = env.param("stations", "")
        if stations:
            args += ["--stations", stations]
        args += env.queue_args()
        return args
