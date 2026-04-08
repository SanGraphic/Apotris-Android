#!/usr/bin/env bash

SCRIPT_PATH="./conversionScripts/"

for f in $SCRIPT_PATH
do
    for script in ${f}*.sh
    do
        bash "$script"
    done
done

python3 ./tools/qr.py https://apotris.com ./data/site_qr.bin
python3 ./tools/qr.py https://apotris.com/wiki ./data/wiki_qr.bin
python3 ./tools/qr.py https://apotris.com/donate ./data/paypal_qr.bin
python3 ./tools/qr.py https://discord.com/invite/jQnxmXS7tr ./data/discord_qr.bin
