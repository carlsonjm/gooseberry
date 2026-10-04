#!/usr/bin/env bash
# Fetches the handwriting reader into the given folder, once: Microsoft's TrOCR
# handwriting model, small size, in ONNX (docs/DECISIONS.md § Handwriting is
# read by a small model on the computer). It is the only thing Gooseberry
# fetches, at installation; reading itself never uses the network.
set -euo pipefail

target="${1:?Usage: fetch-reader.sh FOLDER}"
source_url="${GOOSEBERRY_READER_URL:-https://huggingface.co/Xenova/trocr-small-handwritten/resolve/main}"
files=(
    onnx/encoder_model_quantized.onnx
    onnx/decoder_model_quantized.onnx
    tokenizer.json
    generation_config.json
    config.json
)

if [[ -f ${target}/tokenizer.json && -f ${target}/encoder_model_quantized.onnx && -f ${target}/decoder_model_quantized.onnx ]]; then
    echo "The handwriting reader is already in ${target}."
    exit 0
fi

partial="$(mktemp -d "${TMPDIR:-/tmp}/gooseberry-reader-XXXXXX")"
trap 'rm -rf -- "${partial}"' EXIT
for file in "${files[@]}"; do
    echo "Fetching the handwriting reader: ${file##*/}"
    curl --fail --location --silent --show-error --retry 3 -o "${partial}/${file##*/}" "${source_url}/${file}"
done
# All or nothing: a reader missing a part is not put in place.
mkdir -p "${target}"
mv -- "${partial}"/* "${target}/"
echo "The handwriting reader is in ${target}."
