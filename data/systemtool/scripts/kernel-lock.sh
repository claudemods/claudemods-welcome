# $1 = lock | unlock : hold the installed kernel packages back from system updates (IgnorePkg)
mode=$1
kernels=$(pacman -Qq | grep -E '^linux(-lts|-zen|-hardened|-rt|-cachyos[a-z0-9-]*)?(-headers)?$')
if [ -z "$kernels" ]; then
    echo "No kernel packages found."
    exit 1
fi
backup_conf

declare -A held
for p in $(sed -n 's/^IgnorePkg[[:space:]]*=[[:space:]]*//p' "$PACMAN_CONF" | head -1); do held[$p]=1; done
for k in $kernels; do
    if [ "$mode" = lock ]; then held[$k]=1; else unset "held[$k]"; fi
done
new="${!held[*]}"

if grep -q '^IgnorePkg' "$PACMAN_CONF"; then
    if [ -n "$new" ]; then
        sed -i "s|^IgnorePkg.*|IgnorePkg = $new|" "$PACMAN_CONF"
    else
        sed -i "s|^IgnorePkg.*|#IgnorePkg   =|" "$PACMAN_CONF"
    fi
elif [ -n "$new" ]; then
    if grep -q '^#IgnorePkg' "$PACMAN_CONF"; then
        sed -i "0,/^#IgnorePkg.*/s||IgnorePkg = $new|" "$PACMAN_CONF"
    else
        sed -i "/^\[options\]/a IgnorePkg = $new" "$PACMAN_CONF"
    fi
fi

if [ "$mode" = lock ]; then
    echo "Kernel packages locked (excluded from updates):"
else
    echo "Kernel packages unlocked:"
fi
echo "$kernels"
