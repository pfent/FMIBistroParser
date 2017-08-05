# A parser for the FMI bistro's PDF

Since the FMI bistro still does not provide a decent machine readable menu, here's a (probably buggy) perser for the PDF

## build with
I'm using `boost`, `poppler`, and `curl`, so be sure to have them handy for linkage. All three are usually already 
installed on a desktop linux machine... 

```bash
mkdir build
cd build
cmake ..
make
```

## run with
```bash
# parse the HTML for the current PDF
./FMIBistroParser
# alternatively run with a local PDF (mostly for testing)
./FMIBistroParser ~/Downloads/speiseplan-kw31_2017.pdf
```