<?

// Serves the API documentation page (Scalar, api.html) and the OpenAPI spec
// it reads (openapi.json/.yaml). Route registrations for these live at the
// top of index.php, ahead of the rest of the dispatch table, since they're
// framework/meta routes rather than a domain of the API.

function ServeApiDocs() {
    set_include_path(get_include_path() . PATH_SEPARATOR . dirname(__DIR__));
    extract($GLOBALS);
    include __DIR__ . '/../api.php';
    exit;
}

function ServeApiHtml() {
    header('Content-Type: text/html; charset=utf-8');
    readfile(__DIR__ . '/../api.html');
    exit;
}

function ServeOpenApiSpec() {
    $spec = json_decode(file_get_contents(__DIR__ . '/../openapi.json'), true);

    foreach (collectPluginEndpoints() as $ep) {
        $method = strtolower($ep['method']);
        // Every other path in this spec is the full external path (@route
        // docblocks are written with /api/... baked in, and
        // MergeUndocumentedFppdRoutes() below does the same) -- match that,
        // rather than the bare limonade-relative path these are dispatched
        // at internally. servers.url is "/", not "/api", so leaving this one
        // bare made every generated request URL for a PHP plugin route
        // (Scalar's "Try it", copy-as-curl, etc.) 404.
        $path   = '/api/plugin/' . $ep['plugin'] . '/' . $ep['endpoint'];
        if (!isset($spec['paths'][$path])) {
            $spec['paths'][$path] = array();
        }
        if (!isset($spec['paths'][$path][$method])) {
            $spec['paths'][$path][$method] = array(
                'summary'  => $ep['plugin'] . ' - ' . $ep['endpoint'],
                'tags'     => array('Plugins', $ep['plugin']),
                'responses' => array('200' => array('description' => 'Success')),
            );
        }
    }

    // Before the undocumented sweep, so anything a plugin describes for itself
    // is already in the spec by the time that runs and drops out of its
    // "already documented?" check rather than being reported as undocumented.
    MergePluginApiDocs($spec);
    MergeUndocumentedFppdRoutes($spec);

    header('Content-Type: application/json; charset=utf-8');
    // A content validator rather than the base file's stat: the served spec is
    // the file merged with whatever routes the installed plugins contribute, so
    // it changes when a plugin is installed or removed as well as on upgrade.
    // ~250KB, and the docs page fetches all of it every time it opens.
    $body = json_encode($spec, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
    if (fppSendContentCacheValidators($body)) {
        exit;
    }
    echo $body;
    exit;
}

// Plugins that serve HTTP routes from C++ can describe them in an
// apiDocs.json at the top of the plugin directory, and those descriptions are
// merged into the spec here so they appear on the API page like anything else
// rather than under "Undocumented".
//
// Drogon's registry does not record who registered a route, so fppd cannot tell
// us which plugin owns what (see MergeUndocumentedFppdRoutes() below). A file
// beside the plugin's own code sidesteps that: the plugin declares its own
// paths, and ownership is simply which directory the file came from. It also
// means the documentation ships and versions with the plugin, needs no rebuild
// to change, and works for a route that is not reachable when the docs are
// generated - a plugin that is installed but disabled still documents itself.
//
// The format is an OpenAPI fragment, so it composes with the rest of the spec
// with no translation:
//
//   {
//     "paths": {
//       "/Brightness": {
//         "get": {
//           "summary": "Read the current brightness",
//           "responses": { "200": { "description": "Brightness, 0-200" } }
//         }
//       }
//     }
//   }
//
// Paths are written as the plugin registered them with
// FPPPlugins::registerPluginApi() - "/Brightness", not "/api/plugin-apis/
// Brightness". The external prefix is worked out here, the same way the
// undocumented sweep does it, so a plugin does not have to know whether its
// namespace is proxied directly or through the generic passthrough.
//
// Paths the plugin registers with family=true can be documented individually
// here even though only the parent is registered with Drogon: fppd never
// reports the subpaths, so they would otherwise be undocumentable.
function MergePluginApiDocs(&$spec)
{
    global $pluginDirectory;

    // Same list as MergeUndocumentedFppdRoutes() - anything outside it is only
    // reachable through the generic /api/plugin-apis/ passthrough.
    $directProxyNamespaces = array(
        'fppd', 'overlays', 'models', 'commands', 'command', 'commandPresets',
        'player', 'gpio', 'variables', 'aes67', 'opusrtp',
    );

    $baseDir = $pluginDirectory . '/';
    if (!is_dir($baseDir) || !($dir = opendir($baseDir))) {
        return;
    }
    while (($file = readdir($dir)) !== false) {
        if (in_array($file, array('.', '..')) || !is_dir($baseDir . $file)) {
            continue;
        }
        $docFile = $baseDir . $file . '/apiDocs.json';
        if (!is_file($docFile)) {
            continue;
        }
        $docs = json_decode(file_get_contents($docFile), true);
        // A plugin shipping a broken file must not take the whole API page
        // down with it - log and skip, as the PHP endpoint scan does.
        if (!is_array($docs) || !isset($docs['paths']) || !is_array($docs['paths'])) {
            error_log("FPP: ignoring $docFile -- expected an object with a 'paths' member");
            continue;
        }
        foreach ($docs['paths'] as $rawPath => $methods) {
            if (!is_array($methods) || $rawPath === '' || $rawPath[0] !== '/') {
                error_log("FPP: ignoring path '$rawPath' in $docFile -- paths start with '/'");
                continue;
            }
            $namespace = ltrim(strtok($rawPath, '/'), '/');
            $path = in_array($namespace, $directProxyNamespaces, true)
                ? '/api' . $rawPath
                : '/api/plugin-apis' . $rawPath;

            foreach ($methods as $method => $op) {
                $method = strtolower($method);
                if (!is_array($op)) {
                    continue;
                }
                // Never overwrite something FPP documents itself: a plugin
                // cannot claim one of the core paths by describing it.
                if (isset($spec['paths'][$path][$method])) {
                    continue;
                }
                if (!isset($op['tags'])) {
                    $op['tags'] = array('Plugins', $file);
                }
                if (!isset($op['responses'])) {
                    $op['responses'] = array('200' => array('description' => 'Success'));
                }
                $spec['paths'][$path][$method] = $op;
            }
        }
    }
    closedir($dir);
}

// fppd's /internal/pluginApiRoutes lists every route Drogon has registered,
// FPP's own included -- Drogon's registry doesn't record who registered a
// route, so that's everything it can tell us. For each one, try both
// candidate external paths (direct Apache proxy, and the generic
// plugin-apis passthrough that works for literally any fppd path); if
// either is already documented via an @route docblock (PHP or C++), skip
// it. Otherwise report it as undocumented at whichever candidate its
// namespace actually resolves through. In practice the survivors are
// mostly C++ plugin routes (e.g. fpp-brightness's /Brightness) -- but
// nothing here can prove a route is plugin-owned (confirmed live: several
// of FPP's own bare/catch-all routes have no individual docblock either),
// so this is tagged "Undocumented", not "Plugins". Regex artifacts
// (catch-all patterns like "/fppd/.*") are skipped -- not callable paths,
// just how FPP's own routing overlaps get expressed.
function MergeUndocumentedFppdRoutes(&$spec)
{
    // Local, not top-level define()/$GLOBALS -- function bodies execute
    // lazily on call, so declaring these locally (rather than at file scope,
    // where load order relative to other top-level statements would matter)
    // is safe regardless of when this file is required.

    // Bare paths that are registered with Drogon but have no real content on
    // ANY method -- fppd itself 404s on them regardless (confirmed directly
    // against localhost:32322), they exist only because a wildcard sibling
    // needs a parent match. Nothing useful to document, so skip outright.
    $nonFunctionalBareRoutes = array('/fppd', '/command');

    // Bare path + method combos where the handler requires a sub-path
    // segment (a pin/name/id/etc, already documented at that sub-path) and
    // 404s or 400s on the bare form -- unlike the routes above, GET on these
    // same paths IS real (list/status), so this has to be per-method, not
    // per-path. Every one of these confirmed live against a running fppd;
    // don't add to this list, or remove from it, without checking behavior
    // directly rather than assuming from the handler code alone (this
    // session got bitten twice: PixelOverlayManager::render_PUT only
    // handles p1=="overlays", not "models", despite the shared handler
    // function making them look symmetric; Player::render_POST/PUT are
    // unconditionally dead code regardless of path).
    $nonFunctionalMethodPaths = array(
        'POST /gpio', 'POST /commands', 'POST /commandPresets',
        'POST /player', 'PUT /player',
        'PUT /models',
        'POST /variables', 'DELETE /variables',
    );

    // fppd registers this at /fppdws, an entirely separate websocket proxy
    // (see etc/apache2.site's ProxyPass /fppdws ws://...) -- not a REST path
    // under /api/ at all, so it can't be expressed as one here. And this
    // function's own data source lists itself along with everything else --
    // deliberately undocumented (see the comment on its registerHandler()
    // call in httpAPI.cpp), so exclude it explicitly rather than relying on
    // it happening to fall outside $directProxyNamespaces.
    $nonApiRoutes = array('/fppdws', '/internal/pluginApiRoutes');

    // Routes drogon registers with positional placeholders, documented by hand
    // at the same path with readable parameter names. The "already documented?"
    // checks below compare paths literally, so they cannot match "{1}/{2}"
    // against "{PluginName}/{action}" - list them here instead.
    $documentedUnderFriendlyNames = array('/fppd/plugin/{1}/{2}');

    // Namespaces Apache proxies directly from /api/<name>... straight to
    // fppd (see the RewriteRule list in etc/apache2.site). Anything NOT in
    // this list is only reachable externally via the generic
    // /api/plugin-apis/<path> passthrough (RewriteRule
    // ^plugin-apis/(.*)$ ... -- proxies any fppd path verbatim, confirmed
    // against fpp-brightness's /Brightness and a real third-party plugin's
    // /FPPMon, neither of which resolve at bare /api/*).
    $directProxyNamespaces = array(
        'fppd', 'overlays', 'models', 'commands', 'command', 'commandPresets',
        'player', 'gpio', 'variables', 'aes67', 'opusrtp',
    );

    $ctx = stream_context_create(array('http' => array('timeout' => 1)));
    $body = @file_get_contents('http://localhost:32322/internal/pluginApiRoutes', false, $ctx);
    if ($body === false) {
        return;
    }
    $routes = json_decode($body, true);
    if (!is_array($routes)) {
        return;
    }

    foreach ($routes as $route) {
        if (!isset($route['path']) || !isset($route['method'])) {
            continue;
        }
        $rawPath = $route['path'];
        if (strpbrk($rawPath, '.*()|\\') !== false) {
            continue; // regex catch-all, not a real callable path
        }
        if (in_array($rawPath, $nonFunctionalBareRoutes, true)
            || in_array($rawPath, $nonApiRoutes, true)
            || in_array($rawPath, $documentedUnderFriendlyNames, true)) {
            continue;
        }
        if (in_array($route['method'] . ' ' . $rawPath, $nonFunctionalMethodPaths, true)) {
            continue;
        }

        $namespace = ltrim(strtok($rawPath, '/'), '/');
        $directPath = '/api' . $rawPath;
        $pluginApiPath = '/api/plugin-apis' . $rawPath;
        $method = strtolower($route['method']);

        if (isset($spec['paths'][$directPath][$method])) {
            continue; // already documented, whether direct-proxied or PHP-mediated at the same path
        }
        if (isset($spec['paths'][$pluginApiPath][$method])) {
            continue;
        }
        // Drogon handlers answer HEAD as a side effect of handling GET
        // (req->isHead() checks throughout httpAPI.cpp); it's not a
        // separately meaningful operation, so don't report it as its own
        // undocumented route once GET is covered either way.
        if ($method === 'head' && (isset($spec['paths'][$directPath]['get']) || isset($spec['paths'][$pluginApiPath]['get']))) {
            continue;
        }

        $path = in_array($namespace, $directProxyNamespaces, true) ? $directPath : $pluginApiPath;
        if (!isset($spec['paths'][$path])) {
            $spec['paths'][$path] = array();
        }
        $spec['paths'][$path][$method] = array(
            'summary'   => $rawPath . ' (undocumented - see plugin documentation)',
            'tags'      => array('Undocumented'),
            'responses' => array('200' => array('description' => 'Success')),
        );
    }
}

?>
