Computer Networks
L.EIC – FEUP Lab 1 – Protocol Efficiency
1
Lab 1 – Characterisation of the
protocol efficiency
2025/2026
Lab Report Structure
Objective
The purpose of this report is to present an efficiency analysis of the data link protocol
implemented during the first laboratory work of the Computer Networks course.
Introduction
The main objective of the lab is to design and implement a reliable communication protocol that
operates over a serial RS-232 link between two Linux-based computers. Following the principles
of the data link layer, the implemented protocol ensures reliable data transfer through framing,
error detection, acknowledgements, and retransmissions using the Stop-and-Wait Automatic
Repeat Request (ARQ) mechanism. The system integrates a transmitter and a receiver
application that exchange data and control packets over this link, supporting file transfer
operations while maintaining layer independence.
This report should focus on evaluating the efficiency (S) of the implemented protocol under
different transmission conditions. Parameters such as the Frame Error Rate (FER), propagation
delay (Tprop), Baudrate (C) and frame size (L) can be changed, allowing the comparison between
the obtained results and theoretical efficiency models.
Suggestions

- Study the theoretical formulas for efficiency of ARQ protocols (check the 03-data-link-
  layer.pdf slides from theoretical classes);
- Measure the file transference time varying the FER, Tprop, C and L. Vary one parameter
  at a time;
- To vary FER, you can randomly generate errors on I frames;
- To vary Tprop a propagation delay can be considered.
- For each experience, calculate the achieved throughput R, and calculate the measured
  efficiency S;
- Plot the efficiency S for each set of parameters (FER, Tprop, C and L) and discuss the
  results;
- Compare the experimental results with the known theoretical formulas for efficiency, and
  check the validity of the formulas.
  Computer Networks
  L.EIC – FEUP Lab 1 – Protocol Efficiency
  2
  Note: you can use cable.c to perform the protocol efficiency evaluation. The program contains
  options to change the baudrate, the bit error rate (BER) and the propagation delay. Read the
  program instructions that are printed in the terminal when the program starts.
  Structure
  The report should contain the following information:
- Title, Authors
- Summary
- Introduction
- Methodology
- Results
- Conclusions
  Formatting Guidelines
- Length: 3 pages maximum, including figures and tables
- File format: PDF, named “Authros_Lab1_Report.pdf”
- Language: Portuguese or English
- Font size 11pt
- Identify the author names and their class on the first page
- Figures and tables must be numbered and captioned
- Include references only if external sources are cited (optional)
  Writing Tips
- Structure your text by paragraphs. Each paragraph should focus on one clear topic. The
  first sentence must make that topic explicit;
- Keep paragraphs compact and purposeful. Aim for a clear and visually organized layout
  that helps quick reading;
- Write positively. Describe what you implemented, measured, or observed, not what you
  did not do;
- Be precise. Avoid vague terms like fast or many. Use measurable facts instead (e.g., the
  function executes in 200 ms or the code has 800 lines);
- Be concise. After writing, reread and simplify. If you can say the same thing in fewer
  words, do it;
- Avoid redundancy. Do not restate the same idea in multiple ways; rewrite for clarity
  instead;
- Maintain logical flow. Introduce each concept before using it, and always use consistent
  terms for the same idea (e.g., frame, packet, efficiency).
