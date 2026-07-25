class Module:
    daemon = False
    gui = True

    def start(self, env):
        env.log("tpmap cannot be started as a daemon by design; use `terrapulse exec tpmap`.")
        return None

    def supports_aliases(self):
        return True
