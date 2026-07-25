class Module:
    daemon = False
    gui = True

    def start(self, env):
        env.log("tpolv cannot be started as a daemon by design; use `terrapulse exec tpolv`.")
        return None

    def supports_aliases(self):
        return True
