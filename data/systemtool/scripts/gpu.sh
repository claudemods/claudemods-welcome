# $1 = intel | intel-compute | amd | amd-xorg | nvidia-open | nvidia-dkms | nvidia-lts
pkgs=""
add() { pkgs="$pkgs $*"; }
# also install the 32-bit libraries when [multilib] is enabled (Steam, Wine, ...)
add32() { if repo_active multilib; then for p in "$@"; do add "lib32-$p"; done; else echo "(multilib is disabled: skipping 32-bit libraries)"; fi; }
# kernel headers for every installed kernel (needed by dkms modules)
headers() { for k in $(pacman -Qq | grep -E '^linux(-lts|-zen|-hardened|-rt)?$'); do add "$k-headers"; done; }

case "$1" in
    intel)
        add mesa vulkan-intel vulkan-icd-loader intel-media-driver
        add32 mesa vulkan-intel vulkan-icd-loader
        ;;
    intel-compute)
        add mesa vulkan-intel vulkan-icd-loader intel-media-driver intel-compute-runtime
        add32 mesa vulkan-intel vulkan-icd-loader
        ;;
    amd)
        add mesa vulkan-radeon vulkan-icd-loader
        add32 mesa vulkan-radeon vulkan-icd-loader
        ;;
    amd-xorg)
        add mesa vulkan-radeon vulkan-icd-loader xf86-video-amdgpu
        add32 mesa vulkan-radeon vulkan-icd-loader
        ;;
    nvidia-open)
        headers
        add nvidia-open-dkms nvidia-utils nvidia-settings opencl-nvidia libva-nvidia-driver egl-wayland
        add32 nvidia-utils opencl-nvidia
        ;;
    nvidia-dkms)
        headers
        add nvidia-dkms nvidia-utils nvidia-settings opencl-nvidia libva-nvidia-driver egl-wayland
        add32 nvidia-utils opencl-nvidia
        ;;
    nvidia-lts)
        add nvidia-lts nvidia-utils nvidia-settings opencl-nvidia libva-nvidia-driver egl-wayland
        add32 nvidia-utils opencl-nvidia
        ;;
    *) echo "Unknown driver set: $1"; exit 1 ;;
esac

echo "Installing:$pkgs"
pacman -S --needed $pkgs
