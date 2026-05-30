import { test, expect } from '@playwright/test';

const V2 = '/api/v2';

test.describe('statistics', () => {

  test('GET /statistics/usage', async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'SCHEMA' });
    const res = await request.get(`${V2}/statistics/usage`);
    expect(res.status()).toBe(200);
    const body = await res.json();
    // May return null if no stats file exists yet
    expect(body === null || typeof body === 'object').toBe(true);
  });

  test('POST /statistics/usage', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const res = await request.post(`${V2}/statistics/usage`);
    expect(res.status()).toBe(200);
  });

  test('DELETE /statistics/usage', { tag: ['@state'] }, async ({ request }) => {
    test.info().annotations.push({ type: 'tier', description: 'STATE' });
    const res = await request.delete(`${V2}/statistics/usage`);
    expect(res.status()).toBe(200);
  });

});
