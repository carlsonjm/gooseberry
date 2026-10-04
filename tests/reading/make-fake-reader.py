#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Makes the stand-in reader in fake-reader/: two tiny models with the same
inputs and outputs as the TrOCR handwriting reader Gooseberry uses, and a
tokenizer, so a test runs the real reading code on the computer without the
real model. Whatever it is shown, it reads "measure flick velocity", with
"flack" as its runner-up for the second word.

Run with the onnx and numpy Python packages:
    python3 make-fake-reader.py fake-reader
"""
import json
import sys
from pathlib import Path

import numpy as np
import onnx
from onnx import TensorProto, helper, numpy_helper

out = Path(sys.argv[1] if len(sys.argv) > 1 else "fake-reader")
out.mkdir(parents=True, exist_ok=True)

# What ONNX Runtime 1.21, the oldest Gooseberry is built with, reads.
OPSET = [helper.make_opsetid("", 17)]
IR_VERSION = 9
HIDDEN = (1, 4, 8)

# The encoder: any picture becomes a fixed-size state.
encoder = helper.make_graph(
    [
        helper.make_node("ReduceMean", ["pixel_values"], ["mean"], keepdims=1),
        helper.make_node("Reshape", ["mean", "one"], ["flat"]),
        helper.make_node("Expand", ["flat", "hidden_shape"], ["last_hidden_state"]),
    ],
    "encoder",
    [helper.make_tensor_value_info("pixel_values", TensorProto.FLOAT, [1, 3, 384, 384])],
    [helper.make_tensor_value_info("last_hidden_state", TensorProto.FLOAT, list(HIDDEN))],
    [
        numpy_helper.from_array(np.array([1, 1, 1], dtype=np.int64), "one"),
        numpy_helper.from_array(np.array(HIDDEN, dtype=np.int64), "hidden_shape"),
    ],
)

vocab = ["<s>", "<pad>", "</s>", "<unk>", "▁measure", "▁flick", "▁flack", "▁velocity"]
positions = 8
table = np.zeros((positions, len(vocab)), dtype=np.float32)
table[0, 4] = 5  # measure
table[1, 5] = 5  # flick
table[1, 6] = 4  # flack, the runner-up
table[2, 7] = 5  # velocity
table[3:, 2] = 5  # the end

# The decoder: the next word depends only on how many have been read.
decoder = helper.make_graph(
    [
        helper.make_node("Shape", ["input_ids"], ["shape"]),
        helper.make_node("Gather", ["shape", "one_index"], ["length"], axis=0),
        helper.make_node("Range", ["zero", "length", "step"], ["positions"]),
        helper.make_node("Gather", ["table", "positions"], ["rows"], axis=0),
        helper.make_node("Unsqueeze", ["rows", "first_axis"], ["logits_alone"]),
        helper.make_node("ReduceSum", ["encoder_hidden_states"], ["state_sum"], keepdims=0),
        helper.make_node("Mul", ["state_sum", "nothing"], ["state_zero"]),
        helper.make_node("Add", ["logits_alone", "state_zero"], ["logits"]),
    ],
    "decoder",
    [
        helper.make_tensor_value_info("input_ids", TensorProto.INT64, [1, "length"]),
        helper.make_tensor_value_info("encoder_hidden_states", TensorProto.FLOAT, list(HIDDEN)),
    ],
    [helper.make_tensor_value_info("logits", TensorProto.FLOAT, [1, "length", len(vocab)])],
    [
        numpy_helper.from_array(table, "table"),
        numpy_helper.from_array(np.array(1, dtype=np.int64), "one_index"),
        numpy_helper.from_array(np.array(0, dtype=np.int64), "zero"),
        numpy_helper.from_array(np.array(1, dtype=np.int64), "step"),
        numpy_helper.from_array(np.array([0], dtype=np.int64), "first_axis"),
        numpy_helper.from_array(np.array(0, dtype=np.float32), "nothing"),
    ],
)

for graph, name in ((encoder, "encoder_model.onnx"), (decoder, "decoder_model.onnx")):
    model = helper.make_model(graph, opset_imports=OPSET, producer_name="gooseberry-test")
    model.ir_version = IR_VERSION
    onnx.checker.check_model(model)
    onnx.save(model, out / name)

(out / "tokenizer.json").write_text(json.dumps({
    "added_tokens": [{"id": i, "content": t, "special": True} for i, t in enumerate(vocab[:4])],
    "model": {"type": "Unigram", "vocab": [[t, 0.0] for t in vocab]},
}, ensure_ascii=False, indent=1) + "\n")
(out / "generation_config.json").write_text(json.dumps({
    "decoder_start_token_id": 2, "eos_token_id": 2, "pad_token_id": 1, "max_length": 20,
}, indent=1) + "\n")
