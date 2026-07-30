class Module:
    daemon = True
    order = 12

    # SeedLink archiver at the centre (SeisComp3 slarchive role): pull the
    # acquisition node's stream, write miniSEED into the centre's TDS archive, and
    # republish raw. on the local bus so the console's live trace has a source.
    def start_args(self, env):
        server = "%s:%s" % (env.param("backbone_host", "127.0.0.1"),
                            env.param("slink_serve", 18000))
        args = ["--server", server,
                "--net", env.param("arch_net", "TP"),
                "--sta", env.param("arch_sta", "1"),
                "--archive", env.path("var", "tds"),
                "--object", env.param("object", 1),
                "--station", env.param("station", 1),
                "--sensor", env.param("sensor", 1)]
        args += env.queue_args()
        return args
