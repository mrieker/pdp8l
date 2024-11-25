<?php
    // convert signals.csv file to zynq .xdc pin assignment constraints file
    //   php csvtoxdc.php < signals.csv > signals.xdc
    while ($line = fgets (STDIN)) {
        $cols = explode (",", $line);
        $sig = $cols[0];
        $pin = $cols[7];
        if (($pin != "") && ($pin[0] != " ")) {
            if ($pin[1] == "0") {
                $pin = substr ($pin, 0, 1) . substr ($pin, 2);
            }
            echo "set_property PACKAGE_PIN $pin [get_ports ${sig}_0]\n";
            echo "set_property IOSTANDARD LVCMOS33 [get_ports ${sig}_0]\n";
        }
    }
?>
