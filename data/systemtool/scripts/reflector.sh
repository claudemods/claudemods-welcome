# Rank the fastest Arch mirrors with reflector
pacman -S --needed reflector || exit 1
cp /etc/pacman.d/mirrorlist /etc/pacman.d/mirrorlist.bak
reflector --latest 20 --protocol https --sort rate --save /etc/pacman.d/mirrorlist && echo "Mirrorlist updated (backup: mirrorlist.bak)."
