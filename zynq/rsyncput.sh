#!/bin/bash
rm -f *~ *.jou *.log
exec rsync -av ./ ~/nfs/pdp8l/zynq/
