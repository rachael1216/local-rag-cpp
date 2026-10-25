# 操作指南

从一台干净的 Linux 机器开始，到能向小说提问为止。每一步都写了怎么确认它成功了，卡住时先看最后的[常见问题](#7-常见问题)。

目录

1. [环境要求](#1-环境要求)
2. [准备模型](#2-准备模型)
3. [编译](#3-编译)
4. [配置说明](#4-配置说明)
5. [建库](#5-建库)
6. [提问](#6-提问)
7. [常见问题](#7-常见问题)
8. [换成自己的文本](#8-换成自己的文本)

## 1. 环境要求

| 项目 | 要求 | 原因 |
| --- | --- | --- |
| 系统 | Linux（以 Ubuntu 为例） | 作者在 Linux 下开发 |
| 编译器 | GCC 14+，或 Clang 18+ 搭配 libstdc++ 14 | 代码用了 C++23 的 `<print>`，GCC 13 及以下没有这个头文件 |
| CMake | 3.22+ | `CMakeLists.txt` 里声明的最低版本 |
| 内存 | 建议 4 GB 以上 | 两个模型同时加载 |
| 磁盘 | 约 2 GB | 模型约 1.1 GB，加上编译产物 |

先看编译器版本：

```bash
g++ --version
```

如果显示的是 13 或更低，Ubuntu 24.04 上可以这样装：

```bash
sudo apt update
sudo apt install -y build-essential cmake git wget g++-14
```

装好后编译时指定它：`cmake -S . -B build -DCMAKE_CXX_COMPILER=g++-14`。Ubuntu 22.04 的官方源里没有 g++-14，可以用 `ppa:ubuntu-toolchain-r/test`，或者直接换成 24.04。

> 说明：以上 GCC 版本要求来自代码使用的语言特性，作者的编译环境以仓库为准。如果你用的是别的发行版，只要满足"支持 `<print>`"这一条即可。

## 2. 准备模型

需要两个 GGUF 模型，都放在仓库根目录下的 `models/` 里。这个目录已经被 `.gitignore` 忽略，不会误提交。

| 用途 | 文件名 | 大小 | 来源 |
| --- | --- | --- | --- |
| 嵌入（把文字变成向量） | `bge-small-zh-v1.5-f16.gguf` | 约 50 MB | [CompendiumLabs/bge-small-zh-v1.5-gguf](https://huggingface.co/CompendiumLabs/bge-small-zh-v1.5-gguf) |
| 生成（回答问题） | `qwen2.5-1.5b-instruct-q4_k_m.gguf` | 约 1.1 GB | [Qwen/Qwen2.5-1.5B-Instruct-GGUF](https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct-GGUF) |

```bash
mkdir -p models
wget -P models https://huggingface.co/CompendiumLabs/bge-small-zh-v1.5-gguf/resolve/main/bge-small-zh-v1.5-f16.gguf
wget -P models https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct-GGUF/resolve/main/qwen2.5-1.5b-instruct-q4_k_m.gguf
```

下载慢的话，可以在浏览器里下载后手动放进 `models/`。Qwen 那个仓库里有好几个量化版本，注意要下的是文件名带 `q4_k_m` 的，不是 `fp16`，否则和 `config.json` 对不上。

确认：

```bash
ls -lh models/
```

应该能看到上面两个文件。

## 3. 编译

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

第一次会把 `llama_src/` 里的 llama.cpp 一起编译，机器慢的话要十几分钟。后面只改自己的代码时，增量编译很快。

确认：

```bash
ls build/main_rag
```

文件存在就说明编译成功了。

有 NVIDIA 显卡的话，可以在 cmake 那一步加 `-DGGML_CUDA=ON` 来启用 GPU，此时 `config.json` 里的 `n_gpu_layers` 才会真正起作用。纯 CPU 编译时这个值会被忽略。这一项来自 llama.cpp 本身的构建选项，作者尚未在本项目里验证过，遇到问题请参考 llama.cpp 的文档。

## 4. 配置说明

配置文件是仓库根目录的 `config.json`。程序启动时按**当前目录**去找它，找不到就全部使用内置默认值（此时模型路径为空，会报错退出）。

```json
{
  "models": {
    "embedding":   "./models/bge-small-zh-v1.5-f16.gguf",
    "generation":  "./models/qwen2.5-1.5b-instruct-q4_k_m.gguf",
    "n_gpu_layers": 99
  },
  "document":  { "chunk_size": 500, "overlap_size": 50 },
  "retrieval": { "top_k": 3 },
  "generation": {
    "n_ctx": 4096, "n_batch": 4096, "n_ubatch": 512,
    "max_output_tokens": 512,
    "temperature": 0.7, "top_k": 40, "top_p": 0.9
  },
  "embedding": { "n_ctx": 512, "n_batch": 512, "n_ubatch": 512 }
}
```

| 字段 | 默认值 | 说明 |
| --- | --- | --- |
| `models.embedding` | 空 | 嵌入模型路径，相对于运行时的当前目录 |
| `models.generation` | 空 | 生成模型路径 |
| `models.n_gpu_layers` | 99 | 放到 GPU 上的层数，没有 GPU 时无影响 |
| `document.chunk_size` | 500 | 每块的大小，**单位是字节**，一个汉字约 3 字节，所以 500 大约是 160 个字 |
| `document.overlap_size` | 50 | 相邻块的重叠量，不能大于等于 `chunk_size`，否则会被自动改成一半 |
| `retrieval.top_k` | 3 | 每次提问取回的片段数 |
| `generation.n_ctx` | 4096 | 生成模型的上下文长度 |
| `generation.n_batch` / `n_ubatch` | 4096 / 512 | llama.cpp 的批大小，一般不用动 |
| `generation.max_output_tokens` | 512 | 回答的最大长度 |
| `generation.temperature` | 0.7 | 采样温度，越低越保守 |
| `generation.top_k` | 40 | 采样时只保留概率最高的 k 个候选 |
| `generation.top_p` | 0.9 | 核采样阈值 |
| `embedding.n_ctx` 等 | 512 | 嵌入模型的上下文和批大小。bge-small 的输入上限是 512 个 token，所以 `chunk_size` 不宜调得太大 |

什么时候需要重新建库：

- 改了 `document.chunk_size` 或 `document.overlap_size`，必须重新 `--ingest`，旧的库是按旧参数切的。
- 换了嵌入模型，也必须重新建库，不同模型的向量不能混用。
- 改了 `top_k`、采样参数、`max_output_tokens`，不用重建，聊天时输入 `/reload` 即可生效。

## 5. 建库

```bash
./build/main_rag --ingest <文本文件> <数据库文件>
```

用自带的小说：

```bash
./build/main_rag --ingest novel.txt novel.db
```

正常输出：

```text
reading txt
chunk finished: 329 chunks, embedding
embedded 10/329
embedded 20/329
...
embedded 329/329
serializing
serialize finished
```

`embedded i/N` 每处理 10 个块打印一次，所以会出现很多行，这是进度提示。最后看到 `serialize finished` 就说明库已经写入磁盘。

`novel.txt` 约 140 KB，按默认参数大约会切出几百个块，耗时取决于机器，纯 CPU 下请预留几分钟。

确认：

```bash
ls -lh novel.db
```

数据库文件是二进制，里面存着每个块的文本、章节标签和向量，不用手动编辑。`*.db` 已被 `.gitignore` 忽略。

## 6. 提问

```bash
./build/main_rag --chat novel.db
```

启动后会先打印 `Loading database from:novel.db`，然后出现提示符：

```text
提问 >>>
```

输入问题回车。程序会先告诉你检索到了几个片段，再把回答一个词一个词地打印出来：

```text
提问 >>> 桃园三结义是哪三个人？
Retrieved 3 relevant chunks.
（流式输出的回答）
```

聊天里可用的命令：

| 输入 | 作用 |
| --- | --- |
| `exit` 或 `quit` | 退出 |
| `/reload` | 重新读取 `config.json`，并重新加载模型 |
| 直接回车 | 忽略，继续等待输入 |
| `Ctrl+D` | 退出 |

提问的几个建议：

- 问法尽量具体，带上人名、地名或事件，比如"关羽温酒斩谁"比"关羽做了什么"更容易检索到对的片段。
- 每次提问都是独立的，程序不会记住上一个问题，不要用"他"、"那后来呢"这类指代。
- 看到 `根据当前片段无法得出答案`，说明检索到的片段里没有依据。换个问法，或者在 `config.json` 里把 `top_k` 调大到 5，再 `/reload`。

更多提问示例见 [examples/三国演义问答.md](../examples/三国演义问答.md)。

## 7. 常见问题

**`cmake` 报找不到 CMakeLists.txt**
确认你在仓库根目录，并且文件名是 `CMakeLists.txt`（`L` 大写）。Linux 区分大小写。

**编译报 `fatal error: 'print' file not found`**
编译器太旧，见[环境要求](#1-环境要求)。

**运行报 `runtime error` 并提示模型相关的错误**
通常是 `config.json` 里的路径不对。路径是相对**运行时的当前目录**，所以必须在仓库根目录里运行 `./build/main_rag`，而不是进了 `build/` 再运行。同时检查 `models/` 下文件名是否和配置完全一致。

**建库时提示 `Failed to open file`**
文本文件路径写错了，或者没有读权限。

**聊天时提示 `load failed` 或 `Invalid magic number`**
数据库文件不存在，或者不是本程序生成的。重新执行 `--ingest`。

**回答里出现乱码**
文本不是 UTF-8 编码。用 `file -i 文件名` 查看编码，再用 `iconv` 转换：

```bash
iconv -f GBK -t UTF-8 原文件.txt > 新文件.txt
```

**回答很慢**
纯 CPU 推理就是这样。可以把 `max_output_tokens` 调小，或者换成带 GPU 支持的编译（见[编译](#3-编译)）。

**回答和原文对不上**
1.5B 的小模型有时会不严格遵守"只依据片段回答"的要求。回答后对照一下检索到的章节，或者把 `temperature` 调到 0.2 左右再 `/reload`。

**章节标签显示 `Before Text`**
这个块位于第一个章节标题之前，或者文本里没有符合 `第…章/节/回/卷` 或 `Chapter N` 格式的标题。

## 8. 换成自己的文本

1. 准备一个 UTF-8 编码的 txt 文件，比如 `mybook.txt`。
2. 如果希望回答里带章节，让每个章节标题单独占一行，并以"第X章"、"第X回"、"第X节"、"第X卷"或"Chapter N"开头。
3. 建库并提问：

```bash
./build/main_rag --ingest mybook.txt mybook.db
./build/main_rag --chat mybook.db
```

科研文本的例子见 [examples/research-demo.md](../examples/research-demo.md)。

同一份配置可以对应多个数据库，只要嵌入模型和切块参数没换，它们之间互不影响。
