# MQTT Disconnected
FPP is configured to use MQTT but cannot reach the broker.

1. Confirm the broker host/port and credentials on the [MQTT](../../settings.php#settings-mqtt) settings page.
2. Verify the broker is running and reachable from FPP (`ping` the host, check firewall rules).
3. If you do not use MQTT, clear the MQTT Host field to disable it and remove this warning.
