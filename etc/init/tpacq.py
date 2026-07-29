class Module:
    daemon = True
    order = 20

    # Acquisition. source = "sim" | "replay" | a device (COM6, /dev/ttyUSB0).
    # Feeds the co-located SeedLink backbone (tpslinkserver on this node). An
    # acquisition node has no local broker, so it feeds only (--no-bus); a
    # standalone box keeps the bus + archive so its console stays usable.
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

        # Always feed the local backbone (tpslinkserver is co-located).
        args += ["--slink", "127.0.0.1:%s" % env.param("slink_feed", 18001)]
        args += ["--archive", env.path("var", "tds")]

        if env.role == "acquisition":
            args += ["--no-bus"]           # no local broker on an acquisition node
        return args
