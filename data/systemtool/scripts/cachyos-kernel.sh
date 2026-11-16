echo "Installing the CachyOS kernel (needs the CachyOS repositories)..."
pacman -S --needed linux-cachyos linux-cachyos-headers
echo
echo "Regenerate your bootloader config if your boot manager does not do it automatically (see Updates -> Rebuild GRUB)."
