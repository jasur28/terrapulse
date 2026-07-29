class Module:
    daemon = True
    order = 50

    def start_args(self, env):
        return ["--db", env.param("db", "terrapulse.db"),
                "--port", env.param("ws_port", 8080),
                "--tds", env.path("var", "tds")]
