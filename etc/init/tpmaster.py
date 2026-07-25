class Module:
    order = -1
    daemon = True
    core = True

    def start_args(self, env):
        db = env.get("queues.production.processors.messages.dbstore.write", "var/lib/terrapulse.db")
        return ["--db", db]

    def update_config(self, env):
        return 0
