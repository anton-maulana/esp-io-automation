#!/usr/bin/env sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
if [ ! -d web ]; then
  python3 tools/build_web.py --bootstrap
else
  python3 tools/build_web.py
fi
case "${1:-}" in
  firmware)
    pio run -e d1_mini -t upload
    ;;
  fs)
    pio run -e d1_mini -t uploadfs
    ;;
  all)
    pio run -e d1_mini -t upload
    pio run -e d1_mini -t uploadfs
    ;;
  *)
    echo "Usage: ./upload.sh firmware | fs | all" >&2
    exit 2
    ;;
esac
