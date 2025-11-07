# Copilot Instructions for Serial Port Protocol Project

## Project Overview
This project implements a serial port file transfer protocol in C, with a layered architecture:
- **Application Layer** (`src/application_layer.c`): Handles file transfer logic, packetization, and protocol control (START, DATA, END packets).
- **Link Layer** (`src/link_layer.c`): Manages reliable frame transmission, retransmissions, and timeouts over the serial port.
- **Serial Port** (`src/serial_port.c`): Low-level serial port operations.
- **Virtual Cable** (`cable/`): Used for testing; do not modify these files.

## Key Workflows
- **Build:** Use the provided `Makefile`. Do not modify it. Run `make all` to build all binaries.
- **Run:** Use `make run_tx` and `make run_rx` to start transmitter and receiver. Use `make run_cable` to start the virtual cable (requires `socat`).
- **Test:** After transfer, use `make check_files` to compare sent and received files.
- **Clean:** Use `make clean` to remove build artifacts.

## Development Conventions
- **Edit only files in `src/` and `graphs/`**. Do not change files in `bin/`, `cable/`, or the `Makefile`.
- **Main entry point:** `src/main.c` (do not modify). It parses CLI arguments and calls `applicationLayer()`.
- **Application Layer:** Implement protocol logic in `src/application_layer.c`. Use TLV encoding for control packets. Follow the structure in the provided code.
- **Link Layer:** Implement frame handling, retransmissions, and timeouts in `src/link_layer.c`. Use the `LinkLayer` struct and functions as defined in `link_layer.h`.
- **Payload Size:** The maximum payload size is set via `CUSTOM_MAX_PAYLOAD_SIZE` (default 1000). This can be overridden at build/run time.
- **Testing with Noise/Disconnections:** Use the cable program console to simulate unplugging (press 0), adding noise (press 2), or normal operation (press 1).

## Integration Points
- **Serial Port Device:** Use `/dev/ttyS10` for TX and `/dev/ttyS11` for RX (configurable in Makefile).
- **External Dependency:** `socat` is required for the virtual cable.

## Example Commands
- Build: `make all`
- Run cable: `make run_cable`
- Run receiver: `make run_rx`
- Run transmitter: `make run_tx`
- Check files: `make check_files`
- Clean: `make clean`

## References
- See `README.txt` for detailed instructions and workflow.
- See `src/application_layer.c` and `src/link_layer.c` for protocol implementation patterns.

## Task Assistance
- See file `formulas.md` for key formulas related to Stop-and-Wait ARQ efficiency and throughput calculations.
- See file `report_guide.md` for guidance on writing the project report.
- In graphs/ folder you have 4 pngs with the graphs you need to include in the report.

## Guidelines for Report
This guide prompt is designed for GitHub Copilot, leveraging all provided context, guidelines, and implied theoretical knowledge about the Stop-and-Wait ARQ protocol efficiency derived from the sources.

***

## GitHub Copilot Guide Prompt: RCOM Lab 1 Protocol Efficiency Report

**Goal:** Generate a complete, high-quality, professional Lab Report (`report.md`) for the Computer Networks (RCOM) Lab 1, analyzing the efficiency of the implemented Stop-and-Wait Automatic Repeat ReQuest (ARQ) protocol. The report must strictly follow the provided structure and formatting guidelines.

**Context and Access:**
You have access to:
1.  **Project Codebase/Implementation Details:** (Assume this provides specific values for $L$ and protocol overheads, which you should generalize if unknown, but prioritize precision using measurable facts).
2.  **`report_guide.md` (Efficiency Report Guidelines):** Strict adherence is required for structure, content, length, and writing style.
3.  **`formulas.md`:** This file contains the theoretical efficiency formulas for the Stop-and-Wait ARQ protocol, likely including $S = \frac{1}{1 + 2a}$ (without errors) and $S = \frac{1-p_e}{1 + 2a}$ (with errors, where $p_e$ is FER, and $a = T_{prop}/T_f$).
4.  **`graphs/` folder:** Contains 4 high-resolution graphs showing measured efficiency ($S$) vs.:
    *   Frame Error Rate (FER)
    *   Propagation Delay ($T_{prop}$)
    *   Baudrate ($C$)
    *   Frame Size ($L$)

**Output File Name and Format:** `report.md`. (Note: While the guidelines specify PDF output, you are requested to produce a Markdown file. All structural and textual constraints, including length, must still be met in the Markdown output.)

### 1. General Constraints and Writing Style:

1.  **Language and Tone:** English, maintaining a precise, technical, and professional tone.
2.  **Length Constraint:** Ensure the final Markdown output, once converted or viewed, adheres to a maximum length of 3 pages, including figures and tables. Be concise and avoid redundancy.
3.  **Precision:** Use measurable facts and specific values (e.g., "efficiency decreased by 45%" or "the experiment used C = 64 kbit/s") instead of vague terms (e.g., "fast" or "many").
4.  **Flow:** Maintain a strong logical flow. Introduce all technical concepts (like $T_{prop}$, $T_{pac}$, $a$, FER, Throughput $R$, Efficiency $S$) before their discussion.
5.  **Terminology:** Consistently use the term **frame** (Layer 2 unit) and **efficiency** ($S$).

### 2. Report Structure (Mandatory Sections):

Generate the report following this exact structure. Use Markdown headings for structure.

#### A. Title and Authors
*   **Title:** Characterisation of the Protocol Efficiency for RCOM Lab 1
*   **Authors:** [Author Names and Class Identifiers - Use placeholders: `[Student A Name (ID: XXXXX)], [Student B Name (ID: YYYYY)]`]

#### B. Summary
*   Provide a brief, concise overview (1-2 paragraphs) of the report.
*   State the objective (evaluating Stop-and-Wait ARQ efficiency $S$).
*   Summarize the key finding regarding the relationship between measured results and theoretical models, specifically mentioning which parameters caused the most significant degradation (e.g., high $T_{prop}$ or high FER).

#### C. Introduction
*   **Context:** Introduce the purpose of the report: presenting an efficiency analysis of the reliable data link protocol implemented in Lab 1.
*   **Protocol Overview:** Briefly describe the implemented protocol (operates over RS-232, uses framing and error detection, and employs the **Stop-and-Wait ARQ mechanism** for reliable data transfer).
*   **Objective:** Define the core objective: evaluating efficiency ($S$) under varying conditions (Frame Error Rate $FER$, Propagation Delay $T_{prop}$, Baudrate $C$, and Frame Size $L$) and comparing results with theoretical models.

#### D. Methodology
*   **Setup:** Describe the experimental setup (Linux environment, file transfer operations, RS-232 link simulation, and the utilization of `cable.c` for parameter control).
*   **Measured Parameters:** Define the key performance metrics calculated for each experiment: Throughput ($R$) and Measured Efficiency ($S$).
*   **Experimental Procedure:** Detail how the measurements were conducted. State clearly that the file transference time was measured while varying **one parameter at a time**: FER, $T_{prop}$, $C$ (Baudrate), and $L$ (Frame size).
*   **Theoretical Basis:** Briefly introduce the key variables used in the theoretical model, specifically the definition of the dimensionless parameter $a = T_{prop}/T_f$ (where $T_f = L/C$), which is crucial for determining Stop-and-Wait efficiency. Mention that formulas.md contains the exact theoretical formulas used for comparison.

#### E. Results
*   This section must present and discuss the results for all four measured parameters using the data derived from the `graphs/` folder and theoretical calculation.
*   **For each parameter variation (FER, $T_{prop}$, $C$, and $L$):**
    1.  **Graph Inclusion:** Embed the corresponding graph from the `graphs/` folder (using descriptive Markdown image syntax and captions).
    2.  **Theoretical Comparison:** State the relevant theoretical formula or trend (e.g., $S \propto 1/(1+2a)$ or $S \propto (1-p_e)$) derived from `formulas.md` and "03-data-link-layer.pdf".
    3.  **Discussion:** Discuss the relationship observed between the measured $S$ (and $R$) and the varied parameter. Explain *why* the efficiency behaves as it does, relating back to the core principles of Stop-and-Wait ARQ.
        *   *Example Focus:* Discuss how efficiency drops sharply when $a > 1$ (i.e., when $T_{prop} > T_f$), confirming that Stop-and-Wait is inefficient for large $T_{prop}$ or small $L$.
        *   *Example Focus:* Discuss how an increase in FER (Frame Error Ratio, $p_e$) necessitates more retransmissions, drastically reducing efficiency according to $S \propto (1-p_e)$.
    4.  **Validity Check:** Compare the experimental curve to the theoretical curve (if plotted together). Discuss any observed discrepancies between the measured and theoretical efficiencies, attributing the differences to overheads, processing delays, or synchronization issues not captured by the simple queue models.

#### F. Conclusions
*   Summarize the main findings regarding the efficiency of the Stop-and-Wait protocol.
*   Conclude on the effectiveness of the theoretical models in predicting performance for the conditions tested.
*   **Write positively**, focusing on what was demonstrated and observed.

### 3. Review and Formatting Checks:

*   Verify all sections (Title, Summary, Introduction, Methodology, Results, Conclusions) are present.
*   Ensure the explanation of measured results is precise and uses quantitative data points drawn from the provided graphs.
*   Confirm that the key concept of the report—comparing experimental results against known theoretical formulas for ARQ efficiency—is thoroughly addressed.

---
If any section is unclear or missing important project-specific details, please provide feedback to improve these instructions.
