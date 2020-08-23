# rm old files and dirs
rm -rf ../../workdir/interface-model-extractor/model/
rm ../../workdir/interface-model-extractor/already_parsed_*
rm -rf ../../workdir/interface-model-extractor/tmp
rm ../../workdir/interface-model-extractor/raw_structure_types.txt
rm ../../workdir/interface-model-extractor/simplified_typemap_file.txt
rm ../../workdir/interface-model-extractor/used_types.txt

# create related files and dirs
touch  ../../workdir/interface-model-extractor/used_types.txt
touch ../../workdir/interface-model-extractor/raw_structure_types.txt
touch ../../workdir/interface-model-extractor/simplified_typemap_file.txt
mkdir -p ../../workdir/interface-model-extractor/model/service/
mkdir -p ../../workdir/interface-model-extractor/model/function/
mkdir -p ../../workdir/interface-model-extractor/model/structure/parcelable/data/
mkdir -p ../../workdir/interface-model-extractor/model/structure/parcelable/reply/
mkdir -p ../../workdir/interface-model-extractor/model/structure/flattenable/data/
mkdir -p ../../workdir/interface-model-extractor/model/structure/flattenable/reply/
mkdir -p ../../workdir/interface-model-extractor/model/structure/light_flattenable/data/
mkdir -p ../../workdir/interface-model-extractor/model/structure/light_flattenable/reply/
mkdir -p ../../workdir/interface-model-extractor/model/structure/raw/
mkdir -p ../../workdir/interface-model-extractor/model/union/
mkdir -p ../../workdir/interface-model-extractor/model/enumeration/

mkdir -p ../../workdir/interface-model-extractor/tmp

python init.py