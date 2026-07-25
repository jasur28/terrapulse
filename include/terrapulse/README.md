# TerraPulse public headers

This directory is the clean-room TerraPulse equivalent of SeisComp's public
`include/seiscomp` tree. It exposes stable module-facing headers while the
current implementation remains in `src/`.

The mapping is intentional:

- `terrapulse/core` holds base records, inventory and domain types.
- `terrapulse/client` is the common module application/config layer.
- `terrapulse/messaging` and `terrapulse/broker` wrap the TerraPulse bus/master.
- `terrapulse/io` and `terrapulse/recordstream` expose archive and replay input.
- `terrapulse/processing`, `terrapulse/math` and `terrapulse/qc` expose analysis.
- `terrapulse/gui` anchors the QML GUI framework and workbench modules.

Do not copy SeisComp source into this tree. Keep the layout familiar, but the
implementation TerraPulse-specific and accelerometer/structural-health focused.
