class Module:
    order = 0
    daemon = True
    core = True

    def start_args(self, env):
        return ["--db", env.param("db", "terrapulse.db")]

    def update_config(self, env):
        return 0
