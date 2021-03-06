#!/bin/sh
set -e

# Convert the SVG icon source files to base64 encoded Python strings
# containing PNG data, suitable for inlining in frelay-gui.py.

of=icons.py
rm -f $of

for s in *.svg ; do
    echo "$s"
    f="${s%.*}"

# Inkscape can be used as an alternative to rsvg-convert.
#    inkscape -z -e "$f.png" -w16 -h16 "$s"
    rsvg-convert -w 16 -h 16 -f png -o "$f.png" "$s"

    echo "${f}_png = '''" >> $of
    base64 "$f.png" >> $of
    echo "'''" >> $of
    echo -e "${f}_img = PhotoImage(master=root, data=${f}_png)\n" >> $of

    rm -f "$f.png"
done

