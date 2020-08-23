# Dependency Inferer

Dependency inferer will infer inter-transaction variable dependency and interface dependency.

> Note, we have improved the dependency inferer after submitting the paper. For instance, for array variable sessionId in IDrm interface, we will generate dependency according to variable name and variable type.

Before inferring the dependency, please install the following package dependency so as to generate the corresponding interface dependency graph.

```bash
sudo apt install graphviz
```

Then you can run `sh infer_dependency.sh` to infer these dependencies and get the interface dependency graph.

These dependencies will be directed integrated into the model, located in `workdir/interface-model-extractor/model`.

Also, the simplified interface dependency can be seen at `workdir/interface-dependency`.