<?php
    // convert signals.csv file to port definition lines for zynq.vhd
    //   php csvtovhd.php < signals.csv > signals.vhd
    while ($line = fgets (STDIN)) {
        $cols = explode (",", $line);
        $sig  = $cols[0];
        $pin  = $cols[7];
        if (($pin != "") && ($pin[0] != " ")) {
            $dir = "out";
            if ($sig[0] == "b") $dir = "inout";
            if ($sig[0] == "o") $dir = "in";
            if ($dir != "") {
                printf ("            %-14s : %s std_logic;\n", $sig, $dir);
            }
        }
    }
?>
