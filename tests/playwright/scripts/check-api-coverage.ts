import * as fs from 'fs';
import * as path from 'path';

const repoRoot = path.resolve(__dirname, '../../..');

function readRouterPaths(): Array<{ method: string; path: string }> {
  const indexPhp = fs.readFileSync(path.join(repoRoot, 'www/api/index.php'), 'utf8');
  const routes: Array<{ method: string; path: string }> = [];

  // Shared dispatches: dispatch_all('/some/path', 'get', ...)
  const shared = /dispatch_all\s*\(\s*'([^']+)'\s*,\s*'(get|post|put|delete|patch)'/gi;
  let m: RegExpExecArray | null;
  while ((m = shared.exec(indexPhp)) !== null) {
    routes.push({ method: m[2].toUpperCase(), path: `/v2${m[1]}`.toLowerCase() });
  }

  // Explicit v2 routes: dispatch_get('/v2/some/path', ...)
  const simple = /dispatch_(get|post|put|delete|patch)\s*\(\s*'([^']+)'/gi;
  while ((m = simple.exec(indexPhp)) !== null) {
    const routePath = m[2].toLowerCase();
    if (routePath.startsWith('/v2/')) {
      routes.push({ method: m[1].toUpperCase(), path: routePath });
    }
  }

  // Array-form literal paths: dispatch_get(array('/v2/proxy/*/**', array(...)), ...)
  const arrayForm = /dispatch_(get|post|put|delete|patch)\s*\(\s*array\s*\(\s*'([^']+)'/gi;
  while ((m = arrayForm.exec(indexPhp)) !== null) {
    const routePath = m[2].toLowerCase();
    if (routePath.startsWith('/v2/')) {
      routes.push({ method: m[1].toUpperCase(), path: routePath });
    }
  }

  if (indexPhp.includes("dispatch_get(array($prefix . '/proxy/*/**'")) {
    routes.push({ method: 'GET', path: '/v2/proxy/*/**' });
  }

  return routes.map(r => ({ method: r.method, path: normalizeV2Path(r.path) }));
}

function readOpenApiPaths(): Array<{ method: string; path: string }> {
  const raw = fs.readFileSync(path.join(repoRoot, 'www/api/v2/openapi.json'), 'utf8');
  const spec = JSON.parse(raw);
  const routes: Array<{ method: string; path: string }> = [];
  for (const [rawPath, methods] of Object.entries(spec.paths || {})) {
    const normalized = normalizeSpecPath(rawPath.toLowerCase());
    for (const method of Object.keys(methods as object)) {
      if (['get', 'post', 'put', 'delete', 'patch'].includes(method.toLowerCase())) {
        routes.push({ method: method.toUpperCase(), path: normalized });
      }
    }
  }
  return routes;
}

function readCoveragePaths(): Array<{ method: string; path: string }> {
  const md = fs.readFileSync(path.join(repoRoot, 'tests/playwright/API_COVERAGE.md'), 'utf8');
  const routes: Array<{ method: string; path: string }> = [];
  // Match table rows: | METHOD | /path | ...
  const re = /^\|\s*(GET|POST|PUT|DELETE|PATCH)\s*\|\s*([^\s|]+)\s*\|/gim;
  let m: RegExpExecArray | null;
  while ((m = re.exec(md)) !== null) {
    routes.push({ method: m[1].toUpperCase(), path: normalizeComparablePath(m[2].toLowerCase()) });
  }
  return routes;
}

function key(method: string, p: string): string {
  return `${method.toUpperCase()} ${normalizeComparablePath(p.toLowerCase())}`;
}

function normalizeV2Path(p: string): string {
  if (p === '/v2') return '/';
  if (p.startsWith('/v2/')) return normalizeComparablePath(p.slice(3) || '/');
  return normalizeComparablePath(p);
}

function normalizeSpecPath(p: string): string {
  if (p === '/api/v2') return '/';
  if (p.startsWith('/api/v2/')) return normalizeComparablePath(p.slice('/api/v2'.length) || '/');
  return normalizeComparablePath(p);
}

function normalizeComparablePath(p: string): string {
  return p
    .replace(/\{[^}]+\}/g, '{}')
    .replace(/:[^/]+/g, '{}')
    .replace(/\/\*\*/g, '/{}')
    .replace(/\/\*/g, '/{}')
    .replace(/\/+$/g, '') || '/';
}

function main() {
  const routerRoutes = readRouterPaths();
  const openApiRoutes = readOpenApiPaths();
  const coverageRoutes = readCoveragePaths();

  const routerKeys = new Set(routerRoutes.map(r => key(r.method, r.path)));
  const coverageKeys = new Set(coverageRoutes.map(r => key(r.method, r.path)));
  const openApiKeys = new Set(openApiRoutes.map(r => key(r.method, r.path)));

  const missing = routerRoutes.filter(r => !coverageKeys.has(key(r.method, r.path)));
  const stale = coverageRoutes.filter(r => !routerKeys.has(key(r.method, r.path)));
  const specOnly = openApiRoutes.filter(r => !routerKeys.has(key(r.method, r.path)));

  console.log('\n=== API Coverage Check ===\n');

  if (missing.length > 0) {
    console.log(`MISSING from API_COVERAGE.md (${missing.length}):`);
    for (const r of missing) {
      console.log(`  ${r.method} ${r.path}`);
    }
    console.log();
  } else {
    console.log('MISSING from API_COVERAGE.md: none\n');
  }

  if (stale.length > 0) {
    console.log(`Stale in API_COVERAGE.md — not in router (${stale.length}):`);
    for (const r of stale) {
      console.log(`  ${r.method} ${r.path}`);
    }
    console.log();
  } else {
    console.log('Stale in API_COVERAGE.md: none\n');
  }

  if (specOnly.length > 0) {
    console.log(`In openapi.json but NOT in router (${specOnly.length}):`);
    for (const r of specOnly) {
      console.log(`  ${r.method} ${r.path}`);
    }
    console.log();
  } else {
    console.log('In openapi.json but NOT in router: none\n');
  }

  console.log(
    `Summary: ${routerRoutes.length} router routes, ${coverageRoutes.length} coverage entries, ${openApiRoutes.length} openapi entries`
  );

  if (missing.length > 0) {
    process.exit(1);
  }
}

main();
