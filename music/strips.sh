./nowow  < music.pal > x1
./nonwow < x1 > x2
./nowow  'IFNZRO CPU-CPU8I'  < x2 > x3
./nonwow 'IFZERO CPU-CPU8I'  < x3 > x4
./nowow  'IFZERO CPU-CPU8E'  < x4 > x5
./nonwow 'IFNZRO CPU-CPU8E'  < x5 > x6
./nowow  'IFZERO CORE-MEM4K' < x6 > x7
./nonwow 'IFNZRO CORE-MEM4K' < x7 > x8
./nowow  'IFZERO OS8-HASPTR' < x8 > x9
./nonwow 'IFZERO OS8-HASOS8' < x9 > x10
./nowow  'IFNZRO OS8-HASOS8' < x10 > x11
./nonwow 'IFNZRO CORE-MEM4K' < x11 > x12
./nonwow 'IFZERO T64' < x12 > x13
./nowow  'IFNZRO T64' < x13 > x14
