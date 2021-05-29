<?php
$tests = array("a", "3.4", "master", "a%3Cscript%3Ealert(%22zer0h%22);%3C/script%3E", "<script>Greg</script>");

foreach ($tests as $key => $val) {
    $check = strip_tags($val);
    echo "$val =>  $check \n";
}

//FILTER_SANITIZE_SPECIAL_CHARS

?>