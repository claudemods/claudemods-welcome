if command -v eggs >/dev/null 2>&1; then
    echo "penguins-eggs is already installed ($(eggs version 2>/dev/null | head -1))."
elif pacman -Si penguins-eggs >/dev/null 2>&1; then
    sudo pacman -S --needed penguins-eggs
elif command -v paru >/dev/null 2>&1; then
    paru -S penguins-eggs
elif command -v yay >/dev/null 2>&1; then
    yay -S penguins-eggs
else
    echo "penguins-eggs is not in your enabled repositories. Install it from the AUR (paru/yay) or"
    echo "see https://penguins-eggs.net"
fi
