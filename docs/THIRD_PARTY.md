# Third-party software and models

This project is released under the MIT License (see `LICENSE`). It builds on the work below, each under its own terms. Please check the original pages before you redistribute anything.

## Code included in this repository

| Component | Where | License | Source |
| --- | --- | --- | --- |
| llama.cpp / ggml | `llama_src/` | MIT (see `llama_src/LICENSE`) | https://github.com/ggml-org/llama.cpp |
| nlohmann/json | `json_include/` | MIT (see the header of `json.hpp`) | https://github.com/nlohmann/json |

## Models (downloaded separately, not stored in this repository)

| Model | Used for | License | Source |
| --- | --- | --- | --- |
| bge-small-zh-v1.5 (GGUF conversion by CompendiumLabs) | Turning text into vectors | MIT, as stated on the original model card | https://huggingface.co/BAAI/bge-small-zh-v1.5 |
| Qwen2.5-1.5B-Instruct (GGUF) | Writing answers | Apache-2.0, as stated on the model card at the time of writing | https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct |

Model licenses can change and can differ between sizes of the same family. Read the current model card before you use a model for anything beyond personal study.

## Example texts

| File | Status |
| --- | --- |
| `novel.txt` (first ten chapters of *Romance of the Three Kingdoms* by Luo Guanzhong) | The original work is in the public domain. The text file is a copy of a widely circulated edition. |
| `examples/research_demo.txt` | Written for this project. It is fictional and carries the same MIT License as the code. |
