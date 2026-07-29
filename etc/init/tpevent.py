class Module:
    daemon = True
    order = 40

    # Event association reads results off the broker; no extra arguments.
    def start_args(self, env):
        return []
