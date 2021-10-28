# Custom Fuzzer and Greybox Fuzzer Comparsion

Please see the assignment handout which contains all the essential
information.

Structure of this repository:

* src/original/: -  where the code for the original application lives
* src/vuln-1 -- src/vuln-5 - where your vulnerable versions will live
* poc/:        -  where your PoCs will live
* fuzzer/:     -  where your fuzzer will live
* bin/:        -  where your compiled programs will live
* tests/:      -  where your generated tests will live

Pre-Included Scripts:

* Makefile         - makefile for building the C implementation etc.
* get_coverage.sh  - script to generate coverage reports
* run_fuzzer.sh    - script for running your fuzzer to generate inputs 
* run_tests.sh     - script for running your generated tests against compiled programs 

Vulnerable Versions (you should put your security vulnerabilities in here):

* src/vuln-1/dc.c -- src/vuln-5/dc.c

Proofs of Concept (PoCs that you should provide for each vulnerability):

* poc/vuln-1.poc -- poc/vuln-5.poc

## Introduction

In this group-based assignment, you will gain experience with security testing applied to a small
but non-trivial program, including investigating the trade-offs between different fuzzer designs
and contemporary off-the-shelf greybox fuzzers. You will also learn about memory-error vulnerabilities in C, and use LLVM’s memory sanitisers. You will implement a custom fuzzer to find memory-error vulnerabilities in a C program and insert your own vulnerabilities. Your fuzzer will then “play” competitively against the fuzzers of other teams to determine whose can find the most vulnerabilities. You will also compare your fuzzer against LLVM’s general-purpose greybox fuzzer, libFuzzer, and evaluate it using standard coverage metrics, as well as other metrics that you should decide upon. You will produce a report detailing your vulnerabilities, your fuzzer’s design and systematically evaluating its performance and the implications of your design decisions.