sed '/^<!-- TOC -->$/,/^<!-- TOC -->$/d' nego-engine-broker-fr.md | \
  pandoc -o nego-engine-broker-fr.pdf \
    --pdf-engine=xelatex \
    --lua-filter=mermaid-filter.lua \
    --toc --toc-depth=3 \
    -V lang=fr -V geometry:margin=2cm -V papersize=a4
