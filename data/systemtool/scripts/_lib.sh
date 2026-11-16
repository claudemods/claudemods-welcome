# Shared helpers, prepended to every system tool script that runs as root.
PACMAN_CONF=/etc/pacman.conf

backup_conf() {
    cp "$PACMAN_CONF" "$PACMAN_CONF.bak" && echo "Backup written to $PACMAN_CONF.bak"
}

repo_active()  { grep -q "^\[$1\]" "$PACMAN_CONF"; }
repo_present() { grep -q "^#\?\[$1\]" "$PACMAN_CONF"; }

# enable_repo <name> [before-section]
# Uncomments "[name]" (and its Include line). If the section does not exist it is added in front of
# [before-section] (testing repositories must come before the stable ones), or at the end.
enable_repo() {
    local repo=$1 before=$2
    if repo_active "$repo"; then
        echo "[$repo] is already enabled"
    elif grep -q "^#\[$repo\]" "$PACMAN_CONF"; then
        sed -i "s/^#\[$repo\]/[$repo]/" "$PACMAN_CONF"
        sed -i "/^\[$repo\]/{n;s/^#Include/Include/}" "$PACMAN_CONF"
        echo "[$repo] enabled"
    elif [ -n "$before" ] && repo_active "$before"; then
        sed -i "/^\[$before\]/i [$repo]\nInclude = /etc/pacman.d/mirrorlist\n" "$PACMAN_CONF"
        echo "[$repo] added before [$before]"
    else
        printf '\n[%s]\nInclude = /etc/pacman.d/mirrorlist\n' "$repo" >> "$PACMAN_CONF"
        echo "[$repo] added"
    fi
}

disable_repo() {
    local repo=$1
    if repo_active "$repo"; then
        sed -i "s/^\[$repo\]/#[$repo]/" "$PACMAN_CONF"
        sed -i "/^#\[$repo\]/{n;s/^Include/#Include/}" "$PACMAN_CONF"
        echo "[$repo] disabled"
    fi
}

refresh_hint() {
    pacman -Sy
    echo
    echo "Package databases refreshed. Run 'System update' now before installing anything else."
}
