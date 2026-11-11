#!/usr/bin/env bash
# Portable bounded-model runner. No Codex/skill installation is required.
set -euo pipefail
TLA_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$TLA_DIR/.." && pwd)"
JAR="${TLA_TOOLS_JAR:-$TLA_DIR/tools/tla2tools.jar}"
EXPECTED_SHA=936a262061c914694dfd669a543be24573c45d5aa0ff20a8b96b23d01e050e88
JAR_URL=https://github.com/tlaplus/tlaplus/releases/download/v1.7.4/tla2tools.jar
BUILD_DIR="$PROJECT_DIR/build-tla"

digest() {
    if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}'
    else shasum -a 256 "$1" | awk '{print $1}'; fi
}

if ! command -v java >/dev/null 2>&1; then
    echo 'Java is required (JRE 17+ recommended).' >&2
    exit 2
fi
if [[ ! -f "$JAR" ]]; then
    mkdir -p "$(dirname "$JAR")"
    TEMP_JAR="$JAR.download.$$"
    trap 'rm -f "${TEMP_JAR:-}"' EXIT
    curl --fail --location --retry 3 "$JAR_URL" --output "$TEMP_JAR"
    [[ "$(digest "$TEMP_JAR")" == "$EXPECTED_SHA" ]] || { echo 'TLC download checksum mismatch.' >&2; exit 2; }
    mv "$TEMP_JAR" "$JAR"
fi
[[ "$(digest "$JAR")" == "$EXPECTED_SHA" ]] || { echo 'Expected the pinned TLC 1.7.4 jar checksum.' >&2; exit 2; }
JAR="$(cd "$(dirname "$JAR")" && pwd)/$(basename "$JAR")"

# Whole-file fingerprints complement per-line anchors.lock. Re-review affected
# models before updating this file when modeled implementation changes.
while read -r expected path; do
    [[ -n "$expected" && "$expected" != \#* ]] || continue
    [[ -f "$PROJECT_DIR/$path" && "$(digest "$PROJECT_DIR/$path")" == "$expected" ]] || {
        echo "Modeled source changed: $path; re-review the models and source anchors." >&2
        exit 2
    }
done < "$TLA_DIR/source.sha256"

mkdir -p "$BUILD_DIR"
# Fail if a newly added scenario was omitted from the reproducibility manifest.
while IFS= read -r config_path; do
    relative_config="${config_path#$TLA_DIR/}"
    awk -v wanted="$relative_config" '$1 !~ /^#/ && $2 == wanted { found = 1 } END { exit !found }' \
        "$TLA_DIR/models.tsv" || { echo "Unregistered model config: $relative_config" >&2; exit 2; }
done < <(find "$TLA_DIR" -type f -path '*/02_models/*.cfg' | sort)
while read -r module config; do
    [[ -n "$module" && "$module" != \#* ]] || continue
    model_dir="$TLA_DIR/$(dirname "$module")"
    log="$BUILD_DIR/$(basename "$config" .cfg).log"
    scratch="$BUILD_DIR/states-$(basename "$config" .cfg)"
    echo "Checking $(basename "$config")"
    (
        cd "$model_dir"
        java -Xmx2g -XX:+UseParallelGC -cp "$JAR" tlc2.TLC \
            -workers 1 -coverage 1 -metadir "$scratch" \
            -config "$(basename "$config")" "$(basename "$module")"
    ) > "$log" 2>&1 || { cat "$log"; rm -rf "$scratch"; exit 1; }
    rm -rf "$scratch"
    grep -F 'Model checking completed. No error has been found.' "$log"
    grep -E '^[0-9]+ states generated|^The depth of|^Finished in' "$log"
    if grep -E '^<[^>]+>: [0-9]+:0$' "$log"; then
        echo 'An action generated no states; review model coverage.' >&2
        exit 1
    fi
done < "$TLA_DIR/models.tsv"
echo "All configured bounded checks passed. Logs: $BUILD_DIR"
