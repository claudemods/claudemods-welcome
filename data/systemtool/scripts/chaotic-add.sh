echo "Adding the Chaotic-AUR repository (prebuilt AUR packages)..."
if repo_active chaotic-aur; then echo "[chaotic-aur] is already enabled."; exit 0; fi
pacman-key --recv-key 3056513887B78AEB --keyserver keyserver.ubuntu.com || { echo "Could not fetch the signing key."; exit 1; }
pacman-key --lsign-key 3056513887B78AEB
pacman -U --noconfirm "https://cdn-mirror.chaotic.cx/chaotic-aur/chaotic-keyring.pkg.tar.zst" "https://cdn-mirror.chaotic.cx/chaotic-aur/chaotic-mirrorlist.pkg.tar.zst" || { echo "Could not install the keyring / mirrorlist."; exit 1; }
backup_conf
printf "\n[chaotic-aur]\nInclude = /etc/pacman.d/chaotic-mirrorlist\n" >> "$PACMAN_CONF"
refresh_hint
