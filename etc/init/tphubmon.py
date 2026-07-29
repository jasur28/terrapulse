class Module:
    daemon = True
    order = 50

    def start_args(self, env):
        return ["--port", env.param("hubmon_port", 8081)]
