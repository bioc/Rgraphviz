# R package: Rgraphviz

This is the developer version of Bioconductor package [Rgraphviz](http://bioconductor.org/packages/devel/bioc/html/Rgraphviz.html).  Install in R as:

```r
if (!requireNamespace("BiocManager", quietly=TRUE))
    install.packages("BiocManager")
BiocManager::install('Rgraphviz')
```

# Additional installation tips

Rgraphviz now comes bundles with Graphviz.  This should greatly simplify
installation on all platforms, compared with earlier versions.

While it should not be necessary, we still allow for the use of an external
Graphviz (external to Rgraphviz) at install time, except on Windows (this used
to be supported on Windows but this is no longer true).  This is indicated by
the configure option `--with-graphviz`.

A direct specification of an external Graphviz at install time would be

```{bash}
R CMD INSTALL --configure-args='--with-graphviz=/usr/local' Rgraphviz_*.tar.gz
```
In this case (with $DIR=/usr/local), the configure script assumes the presence
of
```{bash}
$DIR/include/graphviz
$DIR/lib/graphviz
```
as is the default when Graphviz is installed.  It is not possible to directly
specifify these two directories, only `$DIR`

It is also possible to get Rgraphviz to search for Graphviz, by leaving the
`--with-graphviz` option empty, as

```{bash}
R CMD INSTALL --configure-args='--with-graphviz' Rgraphviz_*.tar.gz 
```

In this case, Rgraphviz will search for Graphviz using the following methods
1. First try pkg-config
2. See if Graphviz is in its default location of `/usr/local`

## Notes

The release version of Graphviz 2.28 contains bugs that makes it not work with
Rgraphviz for certain layout options.  The patches to the bundled Graphviz
2.28.0 are included in package.

## R CMD check results
Bioconductor: [Multiple platform build/check report](http://master.bioconductor.org/checkResults/devel/bioc-LATEST/Rgraphviz/)


