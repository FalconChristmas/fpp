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

    MergeUndocumentedFppdRoutes($spec);

    header('Content-Type: application/json; charset=utf-8');
    echo json_encode($spec, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
    exit;
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
            || in_array($rawPath, $nonApiRoutes, true)) {
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
