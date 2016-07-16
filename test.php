<?php
function getMillisecond() {
    list($t1, $t2) = explode(' ', microtime());
    return (float)sprintf('%.5f',(floatval($t1)+floatval($t2))*1000);
}

$dict_name = 'wu';
$text = file_get_contents("/home/maben/sources/c/filter/text.txt");

$start = getMillisecond();
//$result = akm_replace($dict_name, $text, function($keyword, $idx, $extension) {
//});
$result = akm_match($dict_name, $text);
$end = getMillisecond();
var_dump($result);
echo '======' . ($end - $start) . '====';
