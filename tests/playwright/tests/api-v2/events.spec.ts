import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('events', () => {

  test('GET /events', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/events`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    expect(Array.isArray(body)).toBe(true);
  });

  test('GET /events/:eventId', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: an event file to exist on the device with a known eventId
    test.skip(true, 'Requires a pre-existing event file on the device');
  });

  test('POST /events/:eventId/trigger', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    // Requires: an event file to exist on the device with a known eventId
    test.skip(true, 'Requires a pre-existing event file on the device');
  });

});
