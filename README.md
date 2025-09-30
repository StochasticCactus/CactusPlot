# CactusPlot 

## 📖 About

This project started as a quick attempt to solve a small problem of mine.
I don’t know much Rust (to be honest: close to nothing), and the initial version was generated almost entirely by AI.

What began as a hacky experiment slowly grew into something that started resembling an actual application. That said: **the code is probably poorly written, inefficient, and in need of refactoring.**

I’m sharing it here in the open because:

* The app works for me, but could be so much better.
* I’d **love for others to contribute**, improve the code quality, and maybe even turn this into a proper tool.

If you’re experienced in Rust (or just learning and want to practice), this could be a fun project to collaborate on.

---

## ✨ Features

Here are the main things the app currently does:

* **Argument parsing** (`args.rs`):
  Parses command-line options so the app can be run with different modes and inputs.

* **Dataset handling** (`dataset.rs`):
  Loads, manages, and provides access to structured data that powers the application.

* **Data editing** (`data_editor.rs`):
  Provides functionality to manipulate or transform the loaded dataset.

* **Application logic** (`app.rs`):
  Coordinates the major parts of the program — essentially the “core engine.”

* **Utilities** (`utils.rs`):
  Helper functions for common tasks (formatting, validation, etc.).

* **Main entry point** (`main.rs`):
  Wires everything together and launches the app.

---

## 🚧 Current State

* Written mostly by AI, with very little manual Rust experience behind it.
* It works for the original problem I had in mind, but the design is naive.
* No guarantees on efficiency, error handling, or idiomatic Rust style.

Think of this repo as a **prototype** that needs love from the Rust community.

---

## 🤝 Contributing

If you’re interested in:

* Refactoring messy AI-generated Rust code
* Making this more idiomatic and efficient
* Adding features or improving existing ones
* Writing tests and documentation

… then I would be thrilled to see your pull requests!

---

## 🚀 Getting Started

Clone the repo:

```bash
git clone git@github.com:StochasticCactus/CactusPlot.git
cd CactusPlot
```

Build and run:

```bash
cargo run -- [options]
```

*(See `args.rs` for supported CLI arguments.)*

---

📜 License
This project is licensed under the MIT License.
You are free to use, modify, and distribute it as long as you include the original license.

---

## 🙏 Thanks

Huge thanks in advance to anyone who wants to help improve this project.
What started as a quick hack has the potential to grow into something actually useful — with your help!


