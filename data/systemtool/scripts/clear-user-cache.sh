echo "Clearing $HOME/.cache and the Trash..."
rm -rf "$HOME/.cache"/* "$HOME/.local/share/Trash"/* 2>/dev/null
echo "Done."
