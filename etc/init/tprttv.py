class Module:
    daemon = False
    gui = True

    def start(self, env):
        env.log("tprttv cannot be started as a daemon by design; use `terrapulse exec tprttv`.")
        return None

    def supports_aliases(self):
        return True
