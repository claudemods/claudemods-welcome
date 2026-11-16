# $1 = on | off
backup_conf
if [ "$1" = on ]; then
    enable_repo multilib
else
    disable_repo multilib
fi
refresh_hint
