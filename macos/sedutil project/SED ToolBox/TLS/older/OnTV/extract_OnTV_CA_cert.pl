BEGIN {
    $scanning = 0;
    $triggered = 1;
    $copying = 2 ;
    $skipping = 3;
    $state = $scanning;
}

while (<>) {
    if ( $state == $scanning ) {
        $state = $triggered if /subject=.*CN=OnTV Pittsburgh \(Bright Plaza Inc\.\)/ ;
    } 
    if ( $state == $triggered ) {
        $state = $copying if /-----BEGIN CERTIFICATE-----/ ;
    }
    if ( $state == $copying ) {
        print;
        $state = $skipping if /-----END CERTIFICATE-----/ ;
    }
}