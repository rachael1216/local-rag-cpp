<div align="center">

# local-rag-cpp

**Ask questions about your own text files. The answers come from the text, and everything runs on your computer.**

**Rachael Liu (刘瑞琦)**<sup>1</sup> · **Philie Shen (沈曹依)**<sup>1</sup>

<sup>1</sup> Fudan University · [24301050114@m.fudan.edu.cn](mailto:24301050114@m.fudan.edu.cn) · [25303050175@m.fudan.edu.cn](mailto:25303050175@m.fudan.edu.cn)

[简体中文](README.zh-CN.md) · [Guide (中文)](docs/GUIDE.md) · [Examples](examples/) · [License](LICENSE)

![License: MIT](https://img.shields.io/badge/license-MIT-2b3a9e)
![C++23](https://img.shields.io/badge/C%2B%2B-23-2b3a9e)
![Platform: Linux](https://img.shields.io/badge/platform-Linux-2b3a9e)
![Built on llama.cpp](https://img.shields.io/badge/built%20on-llama.cpp-e0742c)

<img src="docs/assets/graphical-abstract.svg" alt="Graphical abstract: a text file is split, turned into vectors and saved. A question finds related passages, and a language model answers from them." width="900">

</div>

## Abstract

local-rag-cpp is a small question answering tool for a single text file. It splits the text into overlapping passages, embeds them with a Chinese embedding model, and keeps them in one binary file. For each question it retrieves the closest passages by cosine similarity and lets a 1.5B instruction model answer from them. Both models run through the llama.cpp C API, so after the download nothing leaves the machine. The repository includes a short fictional research report and the first ten chapters of *Romance of the Three Kingdoms* as examples.

## What it does

You give it a `.txt` file. It cuts the text into short passages, saves them in a database file, and then lets you ask questions in a terminal. For each question it finds the most related passages and asks a small language model to answer from those passages only.

The program calls the llama.cpp C API directly, so there is no Python and no web service. After the two model files are downloaded, you can disconnect from the internet.

- **Answers stay close to the text.** The prompt tells the model to use the retrieved passages and nothing else. If they do not contain the answer, it should say so.
- **Passages remember their chapter.** Chapter titles such as `第三章` or `Chapter 3` are detected while reading, and each passage carries its title to the model.
- **Build once, ask many times.** The database is one binary file you can load again without recomputing anything.
- **Easy to adjust.** Passage size, number of passages per question and sampling settings live in `config.json`. Type `/reload` in the chat to apply changes.

> Small models make mistakes. A 1.5B model can ignore the instruction and answer from memory, or miss a point that is spread over several places. Check important answers against the passages.

## Quick start

You need a Linux machine, CMake 3.22 or newer, and **GCC 14 or newer** (the code uses the C++23 `<print>` header, which GCC 13 does not have). The [guide](docs/GUIDE.md#1-环境要求) explains how to check and install it.

```bash
# 1. Get the code. llama.cpp is already inside llama_src/, so no --recursive.
git clone https://github.com/rachael1216/local-rag-cpp.git
cd local-rag-cpp

# 2. Download the two models into models/ (about 1.2 GB in total)
mkdir -p models
wget -P models https://huggingface.co/CompendiumLabs/bge-small-zh-v1.5-gguf/resolve/main/bge-small-zh-v1.5-f16.gguf
wget -P models https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct-GGUF/resolve/main/qwen2.5-1.5b-instruct-q4_k_m.gguf

# 3. Build. The first run also compiles llama.cpp and takes a few minutes.
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4

# 4. Build a database, then chat
./build/main_rag --ingest examples/research_demo.txt demo.db
./build/main_rag --chat demo.db
```

Run these from the repository root. The program reads `config.json` and the model paths relative to the folder you start it from.

## Example 1: a research report

`examples/research_demo.txt` is a short Chinese report on an invented trial: eight weeks of mindfulness training for university students with poor sleep. It has six chapters, so it shows how chapter labels work. **The study is fictional.** Its numbers are invented for the demo and should not be cited.

<div align="center">
<img src="docs/assets/terminal-demo.svg" alt="Terminal session: the database is built from the demo report, then three questions are asked. The third has no answer in the text and gets a refusal." width="820">
</div>

The picture is an illustration, not a recording. Real answers are worded differently on every run.

The last question is a deliberate trap: the report never mentions melatonin, so the right reply is "the passages do not say". More questions to try, grouped by type, are in [examples/research-demo.md](examples/research-demo.md).

## Example 2: Romance of the Three Kingdoms

`novel.txt` holds the first ten chapters of the classic novel, from the oath in the peach garden to the rise of Cao Cao. It is a longer text with a clear chapter structure.

```bash
./build/main_rag --ingest novel.txt novel.db
./build/main_rag --chat novel.db
```

Questions that work well are about events inside those ten chapters, for example who swore the oath in the peach garden, or why Zhang Fei whipped the inspector. Later events such as Guandu or Red Cliff are not in the file, so the model should decline. See [examples/三国演义问答.md](examples/三国演义问答.md).

## Use your own text

Save your text as UTF-8, put each chapter title on its own line (starting with `第…章`, `第…回`, `第…节`, `第…卷` or `Chapter N`), then run `--ingest` and `--chat` as above. Other encodings show up as garbled characters. Convert them first, for example `iconv -f GBK -t UTF-8 old.txt > new.txt`.

The default embedding model, bge-small-zh, is trained for Chinese. For English or other languages you would swap in a different GGUF embedding model in `config.json` and rebuild the database. This should work, but it has not been tested here.

## How it works

1. **Reading.** The text is read line by line. Lines that look like chapter titles are recorded with their position.
2. **Splitting.** A sliding window cuts the text into passages of about 500 bytes (roughly 160 Chinese characters) with a 50-byte overlap. The cut is moved back to the nearest punctuation mark or line break within the last 100 characters, and never lands in the middle of a character.
3. **Embedding.** The bge model turns each passage into a vector. Passages, chapter titles and vectors are written to one binary file.
4. **Searching.** For a question, the same model makes a vector, and the program compares it with every stored vector by cosine similarity. A small heap keeps the best three. This is a plain scan, which is fine for hundreds of passages and would be slow for millions.
5. **Answering.** The three passages and the question go into a prompt for Qwen2.5-1.5B-Instruct, and the reply is printed as it is generated.

The class design is in [DESIGN.md](DESIGN.md).

## Settings

| Key in `config.json` | Default | Meaning |
| --- | --- | --- |
| `document.chunk_size` | 500 | Passage size in bytes. One Chinese character is about 3 bytes. |
| `document.overlap_size` | 50 | How much neighbouring passages share. |
| `retrieval.top_k` | 3 | Passages given to the model for each question. |
| `generation.max_output_tokens` | 512 | Upper limit on the length of an answer. |
| `generation.temperature` | 0.7 | Lower values give steadier answers. |
| `models.n_gpu_layers` | 99 | Layers placed on a GPU. It does nothing if you build without a GPU. |

If you change `document.*` or the embedding model, build the database again. Other values take effect after `/reload`. The full list is in the [guide](docs/GUIDE.md#4-配置说明).

## Known limits

- Input must be UTF-8, and only one text file can be loaded at a time.
- Generation is slow on a CPU.
- The search compares meaning, so it cannot count or tally things across a whole book.
- Passage cutting only looks at `. ! ? ， 。 ！ ？` and line breaks. Long stretches of dialogue can still be split in awkward places.
- `document.*` settings are not reloaded while the chat is running.
- Only Linux with GCC 14 or newer is a target. macOS and Windows have not been tried.

## Repository layout

```text
src/               source code (reading, splitting, embedding, search, generation)
llama_src/         llama.cpp source
json_include/      nlohmann/json headers
config.json        settings
novel.txt          Example 2
examples/          Example 1 and question lists
docs/              guide, third-party notices, figures
DESIGN.md          class design
```

## Contact

Rachael Liu: [24301050114@m.fudan.edu.cn](mailto:24301050114@m.fudan.edu.cn). Philie Shen: [25303050175@m.fudan.edu.cn](mailto:25303050175@m.fudan.edu.cn). Questions and bug reports are also welcome as GitHub issues.

## Citation

If this project helps your work, you can cite it like this. A `CITATION.cff` file is also included, and GitHub shows a "Cite this repository" button from it.

```bibtex
@software{liu_shen_cpp_local_rag_2026,
  author  = {Liu, Rachael and Shen, Philie},
  title   = {local-rag-cpp: a local question answering tool for your own text files},
  year    = {2026},
  url     = {https://github.com/rachael1216/local-rag-cpp},
  license = {MIT}
}
```

## License

The code in this repository is released under the [MIT License](LICENSE). Copyright (c) 2026 Rachael Liu and Philie Shen, Fudan University.

llama.cpp and nlohmann/json are also MIT licensed. The models are downloaded separately and have their own licenses. Details are in [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md).

## Acknowledgements

Thanks to the [llama.cpp](https://github.com/ggml-org/llama.cpp) project for the inference code, to [nlohmann/json](https://github.com/nlohmann/json) for configuration parsing, and to the teams behind [BGE](https://huggingface.co/BAAI/bge-small-zh-v1.5) and [Qwen2.5](https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct).
