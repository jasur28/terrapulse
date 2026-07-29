class Module:
    daemon = True
    order = 45

    # Alert generation reads anomalies/events off the broker; no extra arguments.
    def start_args(self, env):
        return []
