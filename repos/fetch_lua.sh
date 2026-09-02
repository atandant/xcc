#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEST="$ROOT/repos/lua"
REPOSITORY=https://github.com/lua/lua.git
REVISION=7579fc9d7ed90240487251dfb69168f8e64e9294

if [ ! -e "$DEST" ]; then
    git clone --filter=blob:none "$REPOSITORY" "$DEST"
elif [ ! -d "$DEST/.git" ]; then
    echo "error: $DEST exists but is not a git repository" >&2
    exit 1
fi

origin=$(git -C "$DEST" remote get-url origin)
case "$origin" in
    https://github.com/lua/lua|https://github.com/lua/lua.git|git@github.com:lua/lua.git)
        ;;
    *)
        echo "error: $DEST has unexpected origin: $origin" >&2
        exit 1
        ;;
esac

if [ -n "$(git -C "$DEST" status --porcelain)" ]; then
    echo "error: refusing to replace local changes in $DEST" >&2
    exit 1
fi

if ! git -C "$DEST" cat-file -e "$REVISION^{commit}" 2>/dev/null; then
    git -C "$DEST" fetch --filter=blob:none origin "$REVISION"
fi
git -C "$DEST" checkout --detach "$REVISION"
echo "Lua is ready at $DEST ($REVISION)"
