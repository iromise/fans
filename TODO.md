# TODO

Although FANS can find many vulnerabilities, it can still be improved.

## Code

Improve the code to make FANS more readable and easier to use.

## Interface Model Extractor

Improve the interface model extractor to handle complex situations.

## Fuzzer
### Improve Fuzzer Implementation

- Although we have already provided some input generation strategies according to the variable name and variable type, it is not complete. 
- As for the loop or the array whose size is undetermined, what value should we generate for the corresponding size?
- We need to generate the variable with more semantics (e.g., satisfy dependency) with larger probability, but what should the probability be?
- etc.

### Fuzzing Efficiency

The reflash process is still not fully automated. We still need to do some operations manually. So we'd better provide a fully automated solution to reflash the mobile phone.

### Coverage Guided Fuzzing

Integrate coverage into FANS.

## Fuzzing Other Android Services

For instance, we can extend FANS to fuzz the following services in Android
- Hardware services located in the hardware domain
- Vendor services located in vendor domain
- Java system services

Besides, we may extend FANS to fuzz services which are closed source. We may need to utilize some similar binary analysis techniques to extract the interface model.