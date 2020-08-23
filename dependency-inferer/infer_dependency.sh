mkdir ../workdir/interface-dependency
python infer_dependency.py
dot -Kdot -Tpng -o ../workdir/interface-dependency/interface_dependency.png ../workdir/interface-dependency/interface_dependency.txt
dot -Kdot -Tpdf -o ../workdir/interface-dependency/interface_dependency.pdf ../workdir/interface-dependency/interface_dependency.txt