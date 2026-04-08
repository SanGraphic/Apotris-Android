
BASE_PATH="./sprites/font/"
OUTPUT="./data/"

counter=0
for f in $BASE_PATH
do
    for sprite in ${f}*.png
    do
        ((counter++))
        ./tools/superfamiconv -v -i "$sprite" -p "$OUTPUT"font_pal.bin -t "$OUTPUT"font"$counter"tiles.bin -M gba -D -R
    done
done
