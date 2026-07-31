#!/bin/sh
set -eu

# "jnsm" is retained in this script name for discoverability. The project is
# correctly named jsmn: https://github.com/zserge/jsmn
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEST="$ROOT/repos/jsmn"
REPOSITORY=https://github.com/zserge/jsmn.git
REVISION=25647e692c7906b96ffd2b05ca54c097948e879c

if [ ! -e "$DEST" ]; then
    git clone --filter=blob:none "$REPOSITORY" "$DEST"
elif [ ! -d "$DEST/.git" ]; then
    echo "error: $DEST exists but is not a git repository" >&2
    exit 1
fi

origin=$(git -C "$DEST" remote get-url origin)
case "$origin" in
    https://github.com/zserge/jsmn|https://github.com/zserge/jsmn.git|git@github.com:zserge/jsmn.git)
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
echo "jsmn is ready at $DEST ($REVISION)"
