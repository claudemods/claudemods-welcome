echo "Restarting Plasma shell..."
if systemctl --user restart plasma-plasmashell.service 2>/dev/null; then
    echo "Plasma shell restarted."
else
    kquitapp6 plasmashell 2>/dev/null || killall plasmashell 2>/dev/null
    sleep 1
    (kstart plasmashell >/dev/null 2>&1 || nohup plasmashell >/dev/null 2>&1) &
    echo "Plasma shell restarted."
fi
