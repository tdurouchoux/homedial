> **Not used.** Alerts are soft and visual only (see `spec.md`): there is no alert screen, no takeover, no mute and no pulse. Kept only as a record of the earlier design.

An alert is the state colour of the reading itself:
- On the Room page the value turns `warn` or `danger`.
- On the Sensor page the ring and the StatusLabel (WARN or ALERT) follow the state.
- Nothing needs to be dismissed, nothing takes over the display, and an alert does not wake the screen. It clears when the reading recovers.
