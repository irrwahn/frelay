VI=$1
VH=$2
TAG=$3

VER="$(cat $VI)"
REV="$(svnversion | grep -E '^[0-9]+[^\s]*' | tr ':' '_' )"
if [ -z "$REV" ]; then
    REV="$(git log 2>/dev/null | sed -rn 's!.*git-svn-id.*\@([^ ]*).*!\1!p' | head -n1)"
fi
if [ -n "$REV" ]; then
    VER="$VER.$REV"
fi

echo "$VER"
echo "#define VERSION     \"$VER$TAG\"" > "$VH"
