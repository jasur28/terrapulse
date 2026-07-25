[Unit]
Description=TerraPulse master broker
After=network.target

[Service]
Type=simple
ExecStart=@bindir@/tpmaster --config @sysconfdir@/terrapulse
Restart=on-failure
RestartSec=2

[Install]
WantedBy=multi-user.target
