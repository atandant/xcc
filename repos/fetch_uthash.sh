#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEST="$ROOT/repos/uthash"
REPOSITORY=https://github.com/troydhanson/uthash.git
REVISION=e493aa90a2833b4655927598f169c31cfcdf7861

if [ ! -e "$DEST" ]; then
    git clone --filter=blob:none "$REPOSITORY" "$DEST"
elif [ ! -d "$DEST/.git" ]; then
    echo "error: $DEST exists but is not a git repository" >&2
    exit 1
fi

origin=$(git -C "$DEST" remote get-url origin)
case "$origin" in
    https://github.com/troydhanson/uthash|https://github.com/troydhanson/uthash.git|git@github.com:troydhanson/uthash.git)
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
echo "uthash is ready at $DEST ($REVISION)"
