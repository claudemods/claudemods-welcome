echo "Reinstalling the installed Plasma / KDE workspace packages..."
pkgs=$(pacman -Qq | grep -E "^(plasma-|plasma$|kwin|breeze|kscreen|kscreenlocker|systemsettings|powerdevil|libplasma|libkscreen|libksysguard|kinfocenter|kmenuedit|kde-gtk-config|xdg-desktop-portal-kde|polkit-kde-agent|plasma5support|kglobalacceld|kwayland)")
pacman -S $pkgs
