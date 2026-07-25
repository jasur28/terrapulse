class Environment:
    def __init__(self, root, config=None):
        self.root = root
        self.config = config or {}

    def get(self, key, default=None):
        return self.config.get(key, default)

    def log(self, message):
        print(message)


class Module:
    order = 100
    daemon = True
    gui = False
    core = False

    def start_args(self, env):
        return []

    def update_config(self, env):
        return 0

    def check(self, env):
        return 0


class CoreModule(Module):
    order = 0
    daemon = True
    core = True


class GuiModule(Module):
    daemon = False
    gui = True

    def start(self, env):
        env.log("GUI modules are executed directly, not started as daemons.")
        return None
