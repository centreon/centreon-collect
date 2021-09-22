FROM sphinxdoc/sphinx
RUN pip install sphinx_rtd_theme
COPY sphinx_rtd_theme.css /usr/local/lib/python3.8/site-packages/sphinx_rtd_theme/static/css/theme.css
COPY layout.html /usr/local/lib/python3.8/site-packages/sphinx_rtd_theme/layout.html
COPY versions.html /usr/local/lib/python3.8/site-packages/sphinx_rtd_theme/versions.html
ENTRYPOINT make SPHINXOPTS="-D html_theme=sphinx_rtd_theme" html
