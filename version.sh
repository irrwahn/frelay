VI=$1
VH=$2
TAG=$3

VER="$(cat $VI)"
REV="$(svnversion | grep -E '^[0-9]+[^\s]*' | tr ':' '_' )"
if [ -z "$REV" ]; then
    REV="git$(git show -s --pretty=format:%h)"
fi
if [ -n "$REV" ]; then
    VER="$VER.$REV"
fi

echo "$VER"
echo "#define VERSION     \"$VER$TAG\"" > "$VH"
