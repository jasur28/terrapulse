class Module:
    daemon = True
    order = 5

    def start_args(self, env):
        return ["--file", env.param("inventory", "config/inventory.example.json")]
