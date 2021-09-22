# Middleware data

## Plugin Pack JSON files.

The Plugin Pack JSON files are original PP with some slight
modifications (for testing purposes) that are presented below and that
should be applied whenever JSON files are updated.

### base-generic

The base-generic file should have its information/name property set
to "Base generic" instead of the default "base-generic". This allows the
PP to be presented at the top of the list when used in EMS mode.

### Exchange

It should have its information/status set to "experimental" as the
filters acceptance tests expects an experimental PP which contains the
string "xch".

### Printer

This Plugin Pack is heavily modified. Great care should be taken when
updating this PP, as many data is added or modified. Diff is your
friend !
