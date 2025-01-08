<?php

ob_start();

ob_start();                              // Capturing
phpinfo();                               // phpinfo ()
$info = trim(ob_get_clean());           // output

// Replace white space in ID and NAME attributes... if exists
$info = preg_replace('/(id|name)(=["\'][^ "\']+) ([^ "\']*["\'])/i', '$1$2_$3', $info);

$imp = new DOMImplementation();
$dtd = $imp->createDocumentType(
    'html',
    '-//W3C//DTD XHTML 1.0 Transitional//EN',
    'http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd'
);
$doc = $imp->createDocument(
    'http://www.w3.org/1999/xhtml',
    'html',
    $dtd
);
$doc->encoding = 'utf-8';

$info_doc = new DOMDocument('1.0', 'utf-8');
/* Parse phpinfo's output
 * operator @ used to avoid messages about undefined entities
 * or use loadHTML instead
 */
@$info_doc->loadXML($info);

$doc->documentElement->appendChild( // Adding HEAD element to HTML
    $doc->importNode(
        $info_doc->getElementsByTagName('head')->item(0),
        true                         // With all the subtree
    )
);
$doc->documentElement->appendChild( // Adding BODY element to HTML
    $doc->importNode(
        $info_doc->getElementsByTagName('body')->item(0),
        true                         // With all the subtree
    )
);

// Now you get a clean output and you are able to validate...
/*
echo ($doc->saveXML ());
//      OR
echo ($doc->saveHTML ());
 */

// By that way it's easy to add some style declaration :
$style = $doc->getElementsByTagName('style')->item(0);
$style->appendChild(
    $doc->createTextNode(
        '/* SOME NEW CSS RULES TO ADD TO THE FUNCTION OUTPUT */'
    )
);

// to add some more informations to display :
$body = $doc->getElementsByTagName('body')->item(0);
$element = $doc->createElement('p');
$element->appendChild(
    $doc->createTextNode(
        'FPP cleaned version of phpinfo()'
    )
);
$body->appendChild($element);

// to add a new header :
$head = $doc->getElementsByTagName('head')->item(0);
$meta = $doc->createElement('meta');
$meta->setAttribute('name', 'author');
$meta->setAttribute('content', 'arimbourg at ariworld dot eu');
$head->appendChild($meta);

// As you wish, take the rest of the output and add it for debugging
$out = ob_get_clean();

$pre = $doc->createElement('div'); // or pre
$pre->setAttribute('style', 'white-space: pre;'); // for a div element, useless with pre
$pre->appendChild($doc->createTextNode($out));
$body->appendChild($pre);

$doc->formatOutput = true; // For a nice indentation
//echo ($doc->saveXML());
$exportbody = $doc->documentElement->lastChild;
echo ($doc->saveHTML($exportbody));
?>