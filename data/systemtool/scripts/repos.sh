# $1 = testing | kde-unstable | multilib-testing | stable
backup_conf
case "$1" in
    testing)
        enable_repo core-testing core
        enable_repo extra-testing core
        if repo_active multilib; then enable_repo multilib-testing multilib; fi
        ;;
    kde-unstable)
        enable_repo core-testing core
        enable_repo extra-testing core
        if repo_active multilib; then enable_repo multilib-testing multilib; fi
        enable_repo kde-unstable core
        ;;
    multilib-testing)
        if ! repo_active multilib; then echo "Enable [multilib] first (GPU Drivers -> Enable multilib)."; exit 1; fi
        enable_repo multilib-testing multilib
        ;;
    stable)
        for r in core-testing extra-testing multilib-testing kde-unstable gnome-unstable; do disable_repo "$r"; done
        enable_repo core
        enable_repo extra
        ;;
esac
refresh_hint
