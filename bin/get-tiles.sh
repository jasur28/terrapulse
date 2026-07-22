#!/usr/bin/env bash
# Download OpenStreetMap street tiles into share/maps/osm/{z}/{x}/{y}.png
# (Web-Mercator slippy scheme — the same tree MapView reads).
#
# Usage:
#   ./bin/get-tiles.sh <city> [zmin] [zmax]         # from the Uzbekistan list
#   ./bin/get-tiles.sh bbox LAT1 LAT2 LON1 LON2 [zmin] [zmax]
#   ./bin/get-tiles.sh list                          # show known cities
#
# Be gentle: one or two cities per day. OSM forbids bulk downloading.
set -u
cd "$(dirname "$0")/.."                 # repo root
DEST="share/maps/osm"
UA="TerraPulse/0.1 (structural-health-monitoring; yuldashevj@gmail.com)"

# name  lat1 lat2 lon1 lon2   (city-centre boxes, ~0.2°)
CITIES="
tashkent   41.20 41.40 69.15 69.40
samarkand  39.60 39.72 66.90 67.05
bukhara    39.72 39.82 64.37 64.48
namangan   40.96 41.06 71.58 71.74
andijan    40.72 40.83 72.26 72.42
fergana    40.35 40.43 71.74 71.84
nukus      42.42 42.50 59.55 59.66
qarshi     38.82 38.92 65.74 65.86
kokand     40.50 40.58 70.90 71.02
margilan   40.44 40.51 71.68 71.78
urgench    41.53 41.60 60.58 60.68
khiva      41.35 41.40 60.34 60.40
termez     37.20 37.28 67.24 67.34
jizzakh    40.09 40.17 67.79 67.91
navoiy     40.06 40.14 65.33 65.46
gulistan   40.47 40.53 68.75 68.83
nurafshon  41.00 41.06 69.33 69.43
"

if [ "${1:-}" = "list" ]; then echo "$CITIES" | awk 'NF{print "  "$1}'; exit 0; fi

if [ "${1:-}" = "bbox" ]; then
    LAT1=$2; LAT2=$3; LON1=$4; LON2=$5; ZMIN=${6:-9}; ZMAX=${7:-14}; NAME="bbox"
else
    NAME="${1:-}"; ZMIN=${2:-9}; ZMAX=${3:-14}
    row=$(echo "$CITIES" | awk -v c="$NAME" '$1==c{print; found=1} END{if(!found)exit 1}') || {
        echo "Unknown city '$NAME'. Try: ./bin/get-tiles.sh list"; exit 1; }
    set -- $row; LAT1=$2; LAT2=$3; LON1=$4; LON2=$5
fi

echo "== $NAME  lat[$LAT1..$LAT2] lon[$LON1..$LON2]  z$ZMIN..$ZMAX =="
total=0; got=0; skip=0; fail=0
for Z in $(seq "$ZMIN" "$ZMAX"); do
    r=$(perl -e '
        my ($z,$la1,$la2,$lo1,$lo2)=@ARGV; my $n=2**$z;
        sub yt{my($lat)=@_; my $r=$lat*3.14159265358979/180;
               int((1-log((sin($r)/cos($r))+(1/cos($r)))/3.14159265358979)/2*$n)}
        printf "%d %d %d %d", int(($lo1+180)/360*$n), int(($lo2+180)/360*$n), yt($la2), yt($la1);
    ' "$Z" "$LAT1" "$LAT2" "$LON1" "$LON2")
    set -- $r; X1=$1; X2=$2; Y1=$3; Y2=$4; zc=0
    for X in $(seq "$X1" "$X2"); do
        mkdir -p "$DEST/$Z/$X"
        for Y in $(seq "$Y1" "$Y2"); do
            total=$((total+1)); zc=$((zc+1)); f="$DEST/$Z/$X/$Y.png"
            if [ -s "$f" ]; then skip=$((skip+1)); continue; fi
            if curl -s -f -A "$UA" -o "$f" "https://tile.openstreetmap.org/$Z/$X/$Y.png"; then
                got=$((got+1)); sleep 0.12
            else rm -f "$f"; fail=$((fail+1)); fi
        done
    done
    echo "  z$Z: $zc tiles"
done
echo "== done: total=$total new=$got skipped=$skip failed=$fail  (disk: $(du -sh "$DEST" 2>/dev/null | awk '{print $1}')) =="
