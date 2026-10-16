import { test, expect } from '@playwright/test';
import type { Page, Route } from '@playwright/test';

// The location Lookup buttons when the player cannot reach the internet and the
// browser asks the geolocation services itself.  Runs on initialSetup.php,
// which holds the values until Finish: the Settings page saves them the moment
// they are filled in, which would overwrite the target's real location.
//
// The services are mocked, but the page's real CSP still applies, so a service
// missing from the scoped connect-src fails here the way it would in the field.

const CORS = { 'Access-Control-Allow-Origin': '*' };

// Somewhere far from its zone's reference city (America/Chicago is placed at
// Chicago), so an answer from the service and the offline fallback differ.
const IPWHOIS_ANSWER = {
  ip: '192.0.2.1',
  success: true,
  country: 'United States',
  country_code: 'US',
  latitude: 48.2325,
  longitude: -101.2963,
  is_eu: false,
  timezone: { id: 'America/Chicago' },
};

test.use({ timezoneId: 'America/Chicago', locale: 'en-US' });

// The page keeps polling the player, and a route still installed at teardown
// stalls the trace until the test times out.
test.afterEach(async ({ page }) => {
  await page.unrouteAll({ behavior: 'ignoreErrors' });
});

// The player's own lookup fails, as it does with no route out.  The nolookup
// request the fallback makes afterwards is purely local and goes through.
async function playerOffline(page: Page) {
  await page.route(
    (url) => url.pathname.endsWith('/api/geoip') && !url.searchParams.has('nolookup'),
    (route: Route) => route.fulfill({ status: 502, json: { error: 'GeoIP lookup failed' } })
  );
}

async function clickLookupLocation(page: Page) {
  await page.goto('/initialSetup.php', { waitUntil: 'commit' });
  const button = page.locator("input[value='Lookup Location']");
  await button.waitFor({ state: 'visible', timeout: 10_000 });
  await button.click();
}

test('Location lookup falls back to ipwho.is when ipapi.co is rate limiting', async ({ page }) => {
  await playerOffline(page);
  await page.route('https://ipapi.co/**', (route) =>
    route.fulfill({
      status: 429,
      headers: CORS,
      json: { error: true, reason: 'RateLimited', message: 'Please sign up for a paid plan' },
    })
  );
  await page.route('https://ipwho.is/**', (route) => route.fulfill({ headers: CORS, json: IPWHOIS_ANSWER }));

  await clickLookupLocation(page);

  await expect(page.getByText('looked up from this browser instead')).toBeVisible({ timeout: 20_000 });
  await expect(page.locator('#Latitude')).toHaveValue('48.2325');
  await expect(page.locator('#Longitude')).toHaveValue('-101.2963');
});

test('Location lookup uses ipapi.co when it answers', async ({ page }) => {
  await playerOffline(page);
  await page.route('https://ipapi.co/**', (route) =>
    route.fulfill({
      headers: CORS,
      json: { country_code: 'US', latitude: 46.8772, longitude: -96.7898, timezone: 'America/Chicago' },
    })
  );
  let askedIpwhois = false;
  await page.route('https://ipwho.is/**', (route) => {
    askedIpwhois = true;
    return route.fulfill({ headers: CORS, json: IPWHOIS_ANSWER });
  });

  await clickLookupLocation(page);

  await expect(page.getByText('looked up from this browser instead')).toBeVisible({ timeout: 20_000 });
  await expect(page.locator('#Latitude')).toHaveValue('46.8772');
  expect(askedIpwhois).toBe(false);
});
