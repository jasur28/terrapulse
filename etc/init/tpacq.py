class Module:
    daemon = True
    order = 20

    # Acquisition. source = "sim" | "replay" | a device (COM6, /dev/ttyUSB0).
    # Always feeds the SeedLink backbone; a hub node feeds only (--no-bus), a
    # standalone/centre keeps the bus + archive so the console stays usable.
    def start_args(self, env):
        source = env.param("source", "sim")
        if source == "sim":
            args = ["--sim", "--rate", env.param("rate", 200)]
        elif source == "replay":
            args = ["--replay", env.param("replay", env.path("var", "replay.csv"))]
        else:
            args = ["--port", source, "--baud", env.param("baud", 460800)]

        args += ["--object", env.param("object", 1),
                 "--station", env.param("station", 1),
                 "--sensor", env.param("sensor", 1)]

        feed = "%s:%s" % (env.param("center_host", "127.0.0.1"), env.param("slink_feed", 18001))
        args += ["--slink", feed]

        if env.role == "hub":
            args += ["--no-bus"]
        else:
            args += ["--archive", env.path("var", "tds")]
        return args
