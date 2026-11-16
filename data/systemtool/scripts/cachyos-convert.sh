echo "Converting this Arch system to the CachyOS repositories with the official cachyos-repo script."
echo "It detects your CPU level (v3 / v4 / znver4), installs the keyring and mirrorlists and switches the repos."
tmp=$(mktemp -d) && cd "$tmp" || exit 1
curl -fsSLO https://mirror.cachyos.org/cachyos-repo.tar.xz || { echo "Download failed."; exit 1; }
tar xf cachyos-repo.tar.xz && cd cachyos-repo && ./cachyos-repo.sh
echo
echo "Done. Run System update, then install the CachyOS kernel if you want it."
