echo "Cleaning the pacman package cache..."
if command -v paccache >/dev/null 2>&1; then
    paccache -rk2     # keep the two newest versions of every package
    paccache -ruk0    # drop cached packages that are no longer installed
else
    pacman -Sc
fi
echo "Done."
