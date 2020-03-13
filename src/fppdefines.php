<?

$debug = 0;
$variables = Array();

$headerFiles = [
    "Sequence.h"
    ];

foreach ($headerFiles as $file) {
    $data = file_get_contents('../src/' . $file);

    $lines = preg_split('/\n/', $data);

    foreach ($lines as $line) {
        if (preg_match('/^#define/', $line)) {
            if ($debug)
                echo "Line: $line\n";

            $k = preg_replace('/^#define *(\w+) *.*$/', '$1', $line);

            $v = preg_replace('/^#define *(\w+) *(.*)$/', '$2', $line);

            if ($debug)
                printf( "      '%s' => '%s'\n", $k, $v);

            if ($v != '') {
                if (preg_match('/^[-+\*\/0-9a-zA-Z()_ ]*$/', $v))
                    $v = eval('return ' . $v . ';');

                define("$k", $v);

                if ($debug)
                    printf( "      v => '%s'\n", $v);

                $variables[$k] = $v;
            }
        }
    }
}

# Setup PHP Defines
echo "<?\n";
foreach ($variables as $k => $v) {
    if (is_numeric($v))
        echo "define('$k', $v);\n";
    else if (preg_match('/"/', $v))
        echo "define('$k', $v);\n";
    else
        echo "define('$k', \"$v\");\n";
}
echo "?>\n";

# Setup JavaScript variables
echo "<script>\n";
foreach ($variables as $k => $v) {
    if (is_numeric($v))
        echo "var $k = $v;\n";
    else if (preg_match('/"/', $v))
        echo "var $k = $v;\n";
    else
        echo "var $k = \"$v\";\n";
}
echo "</script>\n";

?>
