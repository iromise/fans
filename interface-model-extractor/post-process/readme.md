# Post Process

The post-process of the interface model extractor will generate a precise interface model.

During this process, we use `python2`.

Here are some required libraries. 

```bash
# Note: you might need to install some other necessary libraries.
pip install --user xmljson
pip install --user lxml
```
After installing these libraries, you should modify the `xmljson` library manually.
First, you should find the `xmljson` library location, then modify it as follows.

```python
# __init__.py file, line 175
### before modifying
        for child in children:
            if count[child.tag] == 1:
                value.update(self.data(child))
            else:
                result = value.setdefault(child.tag, self.list())
                result += self.data(child).values()
### after modifying
        for child in children:
            if count[child.tag] == 1:
                value.update(self.data(child))
            else:
                num = 1
                while True:
                    if child.tag + str(num) in value:
                        num += 1
                    else:
                        break
                child.tag = child.tag + str(num)
                value.update(self.data(child))
                #result = value.setdefault(child.tag, self.list())
                #result += self.data(child).values()
```

Then you can run `sh postprocess.sh` to generate the model. The generated model is located at `workdir/interface-model-extractor/model`.

When you find new functions, e.g.,

```bash
$ sh postprocess.sh
...
Find new function android::IMediaPlayerService::pullBatteryData+android::status_t (class android::Parcel *)
...
```

This means you should collect the files related to those functions described in [Service Related File Collector](../../service-related-file-collector/readme.md).